#define FILTERS_C

#include <math.h>
#include "nespal.h"
#include "filters.h"
#include "global.h"

unsigned rgb_palette[64];
unsigned grayscale_palette[64];

void build_color_maps (void)    
{
    for (int i=0; i<64; i++) {
        rgb_palette[i] = (nes_palette[i*3] << 16) | (nes_palette[i*3+1] << 8) | nes_palette[i*3+2];
        grayscale_palette[i] = (nes_palette[i*3] + nes_palette[i*3+1] + nes_palette[i*3+2]) / 3;        
    }
}

static inline unsigned convert_pixel (byte color, byte emphasis)
{
    emphasis &= 0xE1;
    if (!emphasis) return rgb_palette[color & 63];
    else {
        if (emphasis & 1) color &= 0x30;
        unsigned px = rgb_palette[color & 63];
        byte r = px >> 16, g = (px >> 8) & 0xFF, b = px & 0xFF;
        // This is almost certainly wrong.
        if (emphasis & 0x20) { g = g*3/4; b = b*3/4; }
        if (emphasis & 0x40) { r = r*3/4; b = b*3/4; }
        if (emphasis & 0x80) { r = r*3/4; g = g*3/4; }        
        return (r << 16) | (g << 8) | b;
    }
}

void no_filter_emitter (unsigned y, byte *colors, byte *emphasis)
{
    Uint32 *dest = (Uint32 *)(((byte *)window_surface->pixels) + y * window_surface->pitch);
    for (int x = 0; x < 256; x++) *dest++ = convert_pixel(colors[x], emphasis[x]);
}

void no_filter (void)
{
    vid_width = 256;
    vid_height = 240;
    vid_bpp = 32;
    filter_output_line = no_filter_emitter;
    build_color_maps();
}

void filter_finish_nop (void)
{
}

void rescale_2x_emitter (unsigned y, byte *colors, byte *emphasis)
{
    Uint32 *dest0 = (Uint32 *) (((byte *)window_surface->pixels) + (y*2) * window_surface->pitch);
    Uint32 *dest1 = (Uint32 *) (((byte *)window_surface->pixels) + (y*2+1) * window_surface->pitch);
    for (int x = 0; x < 256; x++) {
        Uint32 px = convert_pixel(colors[x], emphasis[x]);
        *dest0++ = px; 
        *dest0++ = px;
        *dest1++ = px; 
        *dest1++ = px;
    }
}

void rescale_2x (void)
{
    vid_width = 512;
    vid_height = 480;
    vid_bpp = 32;
    filter_output_line = rescale_2x_emitter;
    build_color_maps();
}

/* Scanline filter with alternating fields */

void scanline_emitter (unsigned y, byte *colors, byte *emphasis)
{
    int field = (nes.time & 1);
    Uint32 *dest0 = (Uint32 *) (((byte *)window_surface->pixels) + (y*2+field) * window_surface->pitch);
    Uint32 *dest1 = (Uint32 *) (((byte *)window_surface->pixels) + (y*2+(field^1)) * window_surface->pitch);
    for (int x = 0; x < 256; x++) {
        Uint32 px = convert_pixel(colors[x], emphasis[x]);
        *dest0++ = px; 
        *dest0++ = px;

        Uint32 nx = dest1[0];
        byte r = nx >> 16, g = (nx >> 8) & 0xFF, b = nx & 0xFF;
        r = r*3/4; 
        g = g*3/4; 
        b = b*3/4;
        nx = (r << 16) | (g << 8) | b;
        dest1[1] = dest1[0] = nx;
        dest1 += 2;
    }
}

void scanline_filter (void)
{
    rescale_2x();
    filter_output_line = scanline_emitter;
}

/* NTSC filter */

static inline Uint32 rgbi (byte r, byte g, byte b)
{
    return (r << 16) | (g << 8) | b;
}

static inline Uint32 rgbf (float r, float g, float b)
{
    r *= 255.0;
    if (r < 0.0) r = 0.0;
    if (r > 255.0) r = 255.0;
    g *= 255.0;
    if (g < 0.0) g = 0.0;
    if (g > 255.0) g = 255.0;
    b *= 255.0;
    if (b < 0.0) b = 0.0;
    if (b > 255.0) b = 255.0;

    return rgbi(r,g,b);
}

static inline void yiq2rgb (float yiq[3], float rgb[3])
{
    rgb[0] = yiq[0] +  0.9563*yiq[1] +  0.6210*yiq[2];
    rgb[1] = yiq[0] + -0.2721*yiq[1] + -0.6474*yiq[2];
    rgb[2] = yiq[0] + -1.1070*yiq[1] +  1.7046*yiq[2];
}

float composite_output[64][12];
float y_output[2][3][64][64];
float i_chroma[2][3][64][64];
float q_chroma[2][3][64][64];

/* Size of filter kernels: */
#define KSIZE 513

/* This is the color subcarrier for the I/Q channels. It's really 60
 * samples long - the kernels are generated at 214 MHz, so we can
 * downsample 5:1 to 640 pixels out, but we repeat this to fill the
 * width of the FIR kernels. */
#define SCLEN 513
float kern_i[SCLEN], kern_q[SCLEN];

void ntsc_emitter (unsigned line, byte *colors, byte *emphasis)
{
    Uint32 *dest0 = (Uint32 *) (((byte *)window_surface->pixels) + (line*2) * window_surface->pitch);
    Uint32 *line0 = dest0;
    Uint32 *line1 = (Uint32 *) (((byte *)window_surface->pixels) + (line*2+1) * window_surface->pitch);
    int polarity_mode = ((nes.time ^ line) & 1)? 1 : 0;

#define padding 32
    float ybuf[1280+padding*2];
    float ibuf[1280+padding*2];
    float qbuf[1280+padding*2];

    memset(ybuf, 0, sizeof(ybuf));
    memset(ibuf, 0, sizeof(ibuf));
    memset(qbuf, 0, sizeof(qbuf));

    //printf("%i %i\n", nes.time, line);
    int step_vs_chroma = 2 - ((line + (nes.time & 1)) % 3);

    for (int x=0; x < 256; x++) {
        byte col = colors[x] & 63;

        int off = x & 1;
        float *yseq = &y_output[polarity_mode][step_vs_chroma][col][off];
        float *iseq = &i_chroma[polarity_mode][step_vs_chroma][col][off];
        float *qseq = &q_chroma[polarity_mode][step_vs_chroma][col][off];

        for (int i=off; i<42; i+=2) {
            int idx = padding + x*5 + i - 18;
            ybuf[idx] += *yseq;
            ibuf[idx] += *iseq;
            qbuf[idx] += *qseq;
            yseq+=2;
            iseq+=2;
            qseq+=2;
        }

        step_vs_chroma++;
        if (step_vs_chroma == 3) step_vs_chroma = 0;
    }

    // Filter to YIQ and produce pixels:
    for (int x=0; x<640; x++) {
        float rgb[3];
        float yiq[3] = { 0.0f, 0.0f, 0.0f };
        int cidx = padding + 2*x;

        float scale = 7.5;

        yiq[0] = ybuf[cidx] * scale * 0.7;
        yiq[1] = ibuf[cidx] * scale;
        yiq[2] = qbuf[cidx] * scale;

        yiq2rgb(yiq, rgb); 

        //*dest0++ = rgbf(rgb[0], rgb[1], rgb[2]);

        Uint32 ox = dest0[0];
        const float fadein = 0.6;
        const float fadeout = (1.0 - fadein) * 1.0 / 256.0;
        byte r = ox >> 16, g = (ox >> 8) & 0xFF, b = ox & 0xFF;
        *dest0++ = rgbf(rgb[0]*fadein + r*fadeout,
                        rgb[1]*fadein + g*fadeout,
                        rgb[2]*fadein + b*fadeout);
    }

    // Render alternate scanlines
    for (int x=0; x<640; x++) {
        
        Uint32 nx = line0[x];
        byte r = nx >> 16, g = (nx >> 8) & 0xFF, b = nx & 0xFF;
        r = r>>1; 
        g = g>>1; 
        b = b>>1;
        nx = rgbi(r, g, b);
        line1[x] = nx;
    }

#undef padding
}


void yiq_test_emitter (unsigned y, byte *colors, byte *emphasis);

double blackman (double i, double n)
{
    float p = 2.0 * M_PI * i / n;
    return 0.42 - 0.5*cos(p) + 0.08*cos(2.0*p);
}

double sinc (double f, double i)
{
    if (i == 0.0) return f * 2.0;
    else return sin(2.0*M_PI*f*i) / (i * M_PI);
}

void build_sinc_filter (float *buf, unsigned n, float cutoff)
{
    double sum = 0.0;

    for (int i=0; i<n; i++) {
        buf[i] = blackman(i, n-1) * sinc(cutoff, ((double)i) - 0.5 * ((double)(n-1)));
        sum += buf[i];
    }
}

void downsample_composite (float *y_out, float *i_out, float *q_out, 
                           float *ykern, float *ikern, float *qkern,
                           int modthree, float *chroma)
{
    float ybuf[40+KSIZE];
    float ibuf[40+KSIZE];
    float qbuf[40+KSIZE];
    memset(ybuf, 0, sizeof(ibuf));
    memset(ibuf, 0, sizeof(ibuf));
    memset(qbuf, 0, sizeof(qbuf));    

    for (int i=0; i<40; i+=5) {
        float level = chroma[(i/5 + modthree*8)%12];

        for (int j=0; j<KSIZE; j++) {
            int filter_index = (modthree*40+i)%60;
            ibuf[i+j] += level * ikern[j] * kern_i[filter_index];
            qbuf[i+j] += level * qkern[j] * kern_q[filter_index];
            ybuf[i+j] += level * ykern[j];
        }
    }

    for (int i=0; i<42; i++) {
        int idx = i * 8 + 14*8;
        y_out[i] = ybuf[idx];
        i_out[i] = ibuf[idx];
        q_out[i] = qbuf[idx];
    }
}

void precompute_downsampling (void)
{
    /* The correct cutoff frequencies for the YIQ channels are 3.5
     * MHz, 1.5 MHz, and 0.5 MHz respectively, but my filters suck (I
     * really should measure what's going on or use better ones), so I
     * have to set them a bit lower to keep the leakage under
     * control. */
    float yfilter[KSIZE], ifilter[KSIZE], qfilter[KSIZE];
    build_sinc_filter(yfilter, KSIZE, 0.7 / 60.0);
    build_sinc_filter(ifilter, KSIZE, 0.7 / 142.8);
    build_sinc_filter(qfilter, KSIZE, 0.7 / 428.4);

    for (int polarity=0; polarity<2; polarity++) {
        for (int alignment=0; alignment<3; alignment++) {
            // Precompute colors
            for (int color=0; color<64; color++) {
                downsample_composite(&y_output[polarity][alignment][color][0], 
                                     &i_chroma[polarity][alignment][color][0], 
                                     &q_chroma[polarity][alignment][color][0], 
                                     yfilter, ifilter, qfilter,
                                     alignment,
                                     &composite_output[color][0]);
            }
        }
    }
}

void ntsc_filter (void)
{
    double twelfth = 2.0 * M_PI / 12.0;
    double tint = -1.1;         /* 33 degrees */
    double tint_radians = tint * twelfth;
    memset(composite_output, 0, sizeof(composite_output));

    // Chroma subcarrier
    for (int i=0; i<SCLEN; i++) {
        double phase = ((double)i) * 2.0 * M_PI / 60.0 + tint_radians;
        kern_i[i] = -cos(phase);
        kern_q[i] = -sin(phase);
    }

    // Generate composite waveform at 12 * 3.57 MHz:
    for (int col=0; col<64; col++) {
        int x = col & 15;        
        double black = 0.518;
        double output_black = 0.04;
        double low[4]  = { 0.350, 0.518, 0.962, 1.550 };
        double high[4] = { 1.090, 1.500, 1.960, 1.960 };
        double scale = (1.0 - output_black) / (high[3] - black);

        double *lo = low;
        double *hi = high;

        if (x == 0x0D) hi = low;
        if (x == 0x00) lo = high;
        if (x > 0x0D) scale = 0.0;

        for (int i=0; i< 6; i++) {
            composite_output[col][(i-x-1+24)%12] += output_black + scale * (hi[col>>4] - black);
            composite_output[col][(i-x-1+24+6)%12] += output_black + scale * (lo[col>>4] - black);
        }

    }

    precompute_downsampling();

    vid_width = 768;
    vid_height = 480;
    vid_bpp = 32;
    filter_output_line = ntsc_emitter;
    //filter_output_line = yiq_test_emitter;
}



