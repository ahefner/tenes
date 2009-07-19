#define FILTERS_C

#include <emmintrin.h>
typedef short v8hi __attribute__ ((__vector_size__ (16)));
typedef unsigned char v16qu __attribute__ ((__vector_size__ (16)));

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

float composite_output[8][64][12];
float y_output[8][3][64][42];
float i_chroma[8][3][64][42];
float q_chroma[8][3][64][42];

#define RGB_SCALE 64
#define RGB_SHIFT 6
short __attribute__((aligned(16))) rgb_output[8][3][64][2][22][4];

static inline byte clamp_to_u8 (short x)
{
    x >>= RGB_SHIFT;
    if (x < 0) return 0;
    if (x > 255) return 255;
    else return x;
}

/* Size of filter kernels: */
#define KSIZE 513

/* This is the color subcarrier for the I/Q channels. It's really 60
 * samples long - the kernels are generated at 214 MHz, so we can
 * downsample 5:1 to 640 pixels out, but we repeat this to fill the
 * width of the FIR kernels. */
#define SCLEN KSIZE
float kern_i[SCLEN], kern_q[SCLEN];

void ntsc_emitter (unsigned line, byte *colors, byte *emphasis)
{
    Uint32 *dest0 = (Uint32 *) (((byte *)window_surface->pixels) + (line*2) * window_surface->pitch);
    Uint32 *line0 = dest0;
    //Uint32 *line1 = (Uint32 *) (((byte *)window_surface->pixels) + (line*2+1) * window_surface->pitch);

#define padding 30
    short __attribute__((aligned(16))) vbuf[1280+padding*2][4];
    memset(vbuf, 0, sizeof(vbuf));

    int step_vs_chroma = 2 - ((line + (nes.time & 1)) % 3);

    for (int x=0; x < 256; x++) {
        byte col = colors[x] & 63;
        byte emph = emphasis[x] >> 5;
        if (emphasis[x] & 1) col &= 0x30;

        int off = x & 1;
        short *rgb = &rgb_output[emph][step_vs_chroma][col][off][0][0];

        int idx = (padding + x*5 + off - 18)>>1;

#if 0
        for (int i=0; i<22; i++) {            
            vbuf[idx][0] += rgb[0];
            vbuf[idx][1] += rgb[1];
            vbuf[idx][2] += rgb[2];
            rgb += 4;
            idx++;
        }
#else
        v8hi *in = (v8hi *)rgb;
        v8hi *out = &vbuf[idx][0];
        
        /* Performance of aligned versus unaligned loads: On the Core
         * 2 Quad (2.4 GHz), we gain hugely from switching to aligned
         * loads (at the cost of correctness; we might lose some of
         * that due to memory pressure because we'll double the size
         * of the kernels to support both possible
         * alignments). Tentatively, we go from 9.4s down to 6.7s, but
         * the base work is somewhere around 4.5s, so the speed of
         * this particular operation at least doubles. On the other
         * hand, we're more than fast enough on that machine, whereas
         * my Pentium M 2.0 GHz laptop, whose lackluster performance
         * motivated the SSE rewrite in the first place, hardly gains
         * at all (dropping from ~15s to ~14s), but that's still well
         * faster than real time, so I'm not inclined to spend any
         * more time here.
        */

        // Awful alignment kludge for benchmarking:
        // if ((idx) & 1) out = &vbuf[idx-1][0];

        // if ((((size_t)out)&0xF) != 0) printf("Output pointer not aligned! %p vbuf=%p idx=%i off=%i x=%i\n", out, vbuf, idx, off, x);
        for (int i=0; i<11; i++) {
            // If I fixed the alignment issue, I could do this:
            //printf("out[%i] += in[%i];   out=%p vbuf=%p idx=%i off=%i x=%i\n", i, i, out, vbuf, idx, off, x);            
            //out[i] += in[i];

            v8hi old = __builtin_ia32_loaddqu(out);
            __builtin_ia32_storedqu(out, in[i] + old);
            out++;
        }
#endif        
        step_vs_chroma++;
        if (step_vs_chroma == 3) step_vs_chroma = 0;
    }

    // Output pixels:
/*
    for (int x=0; x<640; x++) {
        int cidx = padding + x;
        byte r = clamp_to_u8(vbuf[cidx][2]);
        byte g = clamp_to_u8(vbuf[cidx][1]);
        byte b = clamp_to_u8(vbuf[cidx][0]);
        Uint32 px = rgbi(r,g,b);
        *dest0++ = px;
    }
*/
    if ((((size_t)dest0)&0xF) != 0) printf("Output pointer not aligned! Fuck!\n");
    if ((((size_t)vbuf)&0xF) != 0) printf("Input pointer not aligned! Fuck!\n");

    __v16qi *out = (v16qu *)dest0;
    for (int x=0; x<640; x+=4) {
        int cidx = padding + x;
        v8hi v1 = *(v8hi *)(&vbuf[cidx][0]);
        v8hi v2 = *(v8hi *)(&vbuf[cidx+2][0]);
        v1 = __builtin_ia32_psrawi128(v1, RGB_SHIFT);
        v2 = __builtin_ia32_psrawi128(v2, RGB_SHIFT);
        *out = __builtin_ia32_packuswb128(v1, v2);
        out++;
    }

    // Interpolate scanlines
    if (line) {
        for (int x=0; x<640; x++) {
            
            Uint32 nx = line0[x];
            nx >>= 1;
            nx &= 0x7F7F7F;
            nx += (line0[x-640-640] >> 1) & 0x7F7F7F;
            line0[x-640] = nx;
        }
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
                           short *rgb_even, short *rgb_odd,
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

        float yiq[3], rgbf[8];
        yiq[0] = ybuf[idx] * 0.7;
        yiq[1] = ibuf[idx];
        yiq[2] = qbuf[idx];
        for (int j=0; j<3; j++) yiq[j] *= 7.5;
        yiq2rgb(yiq, rgbf);
        short *rgb = ((i&1)? rgb_odd : rgb_even) + 4*(i>>1);
        rgb[2] = rgbf[0] * 255.0 * (float)RGB_SCALE;
        rgb[1] = rgbf[1] * 255.0 * (float)RGB_SCALE;
        rgb[0] = rgbf[2] * 255.0 * (float)RGB_SCALE;
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

    for (int emphasis=0; emphasis<8; emphasis++) {
        for (int alignment=0; alignment<3; alignment++) {
            for (int color=0; color<64; color++) {
                downsample_composite(&y_output[emphasis][alignment][color][0], 
                                     &i_chroma[emphasis][alignment][color][0], 
                                     &q_chroma[emphasis][alignment][color][0], 
                                     yfilter, ifilter, qfilter,
                                     &rgb_output[emphasis][alignment][color][0][0][0],
                                     &rgb_output[emphasis][alignment][color][1][0][0],
                                     alignment,
                                     &composite_output[emphasis][color][0]);
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
    memset(rgb_output, 0, sizeof(rgb_output));

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

        for (int emph=0; emph<8; emph++) {
            /* Something quite possibly wrong here. */
            const int emap[3][12] = {{ 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
                                     { 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1 },
                                     { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0 }};

            // Generate color waveform:
            for (int i=0; i<6; i++) 
            {
                composite_output[emph][col][(i-x-1+24)%12] += 
                    output_black + scale * (hi[col>>4] - black);

                composite_output[emph][col][(i-x-1+24+6)%12] += 
                    output_black + scale * (lo[col>>4] - black);
            }
            
            // Apply color emphasis:
            for (int i=0; i<12; i++) {
                float dim = 0.746;
                float em = 1.0;
                if ((emph & 1) && emap[0][i]) em = dim;
                if ((emph & 2) && emap[1][i]) em = dim;
                if ((emph & 4) && emap[2][i]) em = dim;
                composite_output[emph][col][i] *= em;
            }
        }
    }

    precompute_downsampling();

    vid_width = 640;
    vid_height = 480;
    vid_bpp = 32;
    filter_output_line = ntsc_emitter;
}



