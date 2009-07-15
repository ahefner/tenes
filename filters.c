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
    if (r < 0) r = 0.0;
    if (r > 255.0) r = 255.0;
    g *= 255.0;
    if (g < 0) g = 0.0;
    if (g > 255.0) g = 255.0;
    b *= 255.0;
    if (b < 0) b = 0.0;
    if (b > 255.0) b = 255.0;

    return rgbi(r,g,b);
}

static inline void yiq2rgb (float yiq[3], float rgb[3])
{
    rgb[0] = yiq[0] +  0.9563*yiq[1] +  0.6210*yiq[2];
    rgb[1] = yiq[0] + -0.2721*yiq[1] + -0.6474*yiq[2];
    rgb[2] = yiq[0] + -1.1070*yiq[1] +  1.7046*yiq[2];
}

float yiq_table[64][3];
float chroma_output[64][12];
float color_saturation[64];
float kern_i12[12];
float kern_q12[12];
float i_chroma[2][3][64][64];
float q_chroma[2][3][64][64];

#define K160SIZE 769
float kern_sinc_160[K160SIZE];

#define K120SIZE 513
float kern_sinc_120[K120SIZE] ;

#define K90SIZE 385
float kern_sinc_90[K90SIZE];

#define BK90SIZE 513
float bkern_sinc_90[BK90SIZE];

#define K60SIZE 257
float kern_sinc_60[K60SIZE];

#define KCSIZE 65
float kern_i[KCSIZE*2], kern_q[KCSIZE*2];

void ntsc_emitter (unsigned line, byte *colors, byte *emphasis)
{
    Uint32 *dest0 = (Uint32 *) (((byte *)window_surface->pixels) + (line*2) * window_surface->pitch);
    Uint32 *line0 = dest0;
    Uint32 *line1 = (Uint32 *) (((byte *)window_surface->pixels) + (line*2+1) * window_surface->pitch);
    int polarity_mode = ((nes.time ^ line) & 1)? 1 : 0;
    float chroma_polarity = polarity_mode? -1.0 : 1.0;

#define padding 32
    float ybuf[1280+padding*2];   
    float ibuf[1280+padding*2];
    float qbuf[1280+padding*2];

    // Generate video signal:
    memset(ybuf, 0, sizeof(ybuf));
    memset(ibuf, 0, sizeof(ibuf));
    memset(qbuf, 0, sizeof(qbuf));

    int step = 0;
    int stepmod = 0;
    
    int step_vs_chroma = 0;
    
    for (int x=0; x < 256; x++) {
        byte col = colors[x] & 63;
        float y = yiq_table[col][0];
        float total_y = 5.0 * y;

        /* This way of modelling the chroma leakage neglects that the
         * phase of the chroma pulse varies. In fact, this is pretty
         * stupid all around. I could compute a kernel for the
         * downsampled chroma pulse at each possible phase (eight,
         * relative to output resolution) and mix that with
         * downsampled IRs for the luminance component, operating
         * entirely at the output resolution (instead of 2x).  */
        
        if (!((x&1) ^ ((col&15)>8))) total_y += chroma_polarity * color_saturation[col];

        const float ykernel[25] =
            {-0.0016978685, -0.0032530662, -0.0048049833, -0.0052334326, -0.002848882, 0.004248273, 0.017522218, 0.037256636, 0.062008455, 0.08854916, 0.11243146, 0.12907685, 0.13505009, 0.12907685, 0.11243146, 0.08854916, 0.062008455, 0.037256636, 0.017522218, 0.004248273, -0.002848882, -0.0052334326, -0.0048049833, -0.0032530662, -0.0016978685 };

        for (int i=0; i<25; i++) ybuf[padding + x*5 + i - 12] += total_y*ykernel[i];


        for (int i=0; i<42; i++) {
            int idx = padding + x*5 + i - 18;
//            float yib = y * 2.0 * yibleed[step_vs_chroma][i] * chroma_polarity;
//            float yqb = y * 2.0 * yqbleed[step_vs_chroma][i] * chroma_polarity;            
//            float ix = 0.0;
//            float qx = 0.0;

            ibuf[idx] += i_chroma[polarity_mode][step_vs_chroma][col][i];
            qbuf[idx] += q_chroma[polarity_mode][step_vs_chroma][col][i];

        }

        step_vs_chroma++;
        if (step_vs_chroma == 3) step_vs_chroma = 0;
    }

//    printf("------------------------------------------------------\n%i: ", line);
    for (int i=0; i<40; i++) {
//        printf("%3i ", (int)(qbuf[i+640] * 256.0));
    }
//    printf("\n");

   /* Old composite render: */
/*  
        for (int i=0; i<8; i++) {
            float level = yiq_table[col][0] + chroma_polarity * chroma_output[col][stepmod];
            int base = (step+pad)*5;
            rbuf[base+0] = level * 5.0;
            step++;
            stepmod++;
            if (stepmod == 12) stepmod = 0;
        }
*/


    // Lowpass filter
/*
    for (int i=0; i < sizeof(ibuf)/sizeof(ibuf[0]); i++) {
        float si = rbuf[i] * kern_i[i%60] * chroma_polarity;
        float sq = rbuf[i] * kern_q[i%60] * chroma_polarity;

        for (int j=0; j<K120SIZE; j++) {
            ibuf[i+j] += si * kern_sinc_120[j];
            qbuf[i+j] += sq * kern_sinc_120[j];
        }

        for (int j=0; j<K90SIZE; j++) {
            ybuf[i+j] += rbuf[i] * kern_sinc_90[j];
        }
    }
*/

    // Filter to YIQ and produce pixels:
    for (int x=0; x<640; x++) {
        float rgb[3];
        float yiq[3] = { 0.0f, 0.0f, 0.0f };
        //int cidx = 8 + pad*5 + x*16;
        int cidx = padding + 2*x;

        float scale = 12.0 * 0.5;

        yiq[0] = ybuf[cidx];
//        yiq[1] = ibuf[padding + 2*x];
//        yiq[2] = qbuf[padding + 2*x];
//        yiq[1] = 0.0;
//        yiq[2] = 0.0;

        yiq[1] = ibuf[cidx] * scale * 1.7;
        yiq[2] = qbuf[cidx] * scale * 1.7;
        yiq[1] *= yiq[0] * 3.9;
        yiq[2] *= yiq[0] * 3.9;

        yiq2rgb(yiq, rgb); 
        //*dest0++ = rgbf(rgb[0], rgb[1], rgb[2]);


        Uint32 ox = dest0[0];
        byte r = ox >> 16, g = (ox >> 8) & 0xFF, b = ox & 0xFF;
        *dest0++ = rgbf(rgb[0]*0.5 + r/512.0, rgb[1]*0.5 + g/512.0, rgb[2]*0.5 + b/512.0);

    }

    // Render alternate scanlines
    for (int x=0; x<640; x++) {
        
        Uint32 nx = line0[x];
        byte r = nx >> 16, g = (nx >> 8) & 0xFF, b = nx & 0xFF;
        r = r*2/4; 
        g = g*2/4; 
        b = b*2/4;
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

    printf("sinc %i / %f: sums to %f\n", n, cutoff, sum);
}

void precompute_downsampling (void)
{
    float ibuf[40+K120SIZE];
    float qbuf[40+K120SIZE];
    
    for (int polarity=0; polarity<2; polarity++) {
        float polf = polarity? -1.0 : 1.0;
        for (int alignment=0; alignment<3; alignment++) {
            for (int color=0; color<64; color++) {
                memset(ibuf, 0, sizeof(ibuf));
                memset(qbuf, 0, sizeof(qbuf));

                int off = alignment * 40;
                for (int i=0; i<40; i+=5) {
                    float level = yiq_table[color][0] + polf * chroma_output[color][(i/5 + alignment*8)%12];
                    //float ibase = kern_i12[off+i];
                    //float qbase = kern_q12[off+i];
                    for (int j=0; j<K120SIZE; j++) {
                        ibuf[i+j] += level * polf * kern_i[(off+i)%60]*bkern_sinc_90[j];
                        qbuf[i+j] += level * polf * kern_q[(off+i)%60]*bkern_sinc_90[j];
                    }
                }
                
                for (int i=0; i<42; i++) {
                    int idx = i * 8 + 14*8;
                    //printf("%i %i %i %i: %f %f\n", polarity, alignment, color, i, ibuf[idx], qbuf[idx]);
                    i_chroma[polarity][alignment][color][i] = ibuf[idx];
                    q_chroma[polarity][alignment][color][i] = qbuf[idx];
                }
            }
        }
    }
}

void ntsc_filter (void)
{
    double twelfth = 2.0 * M_PI / 12.0;
    double tint = 0.4;
    double tint_radians = tint * twelfth;
    memset(chroma_output, 0, sizeof(chroma_output));

    // Base YIQ sinusoid for testing
    for (int step=0; step<12; step++) {
        double phase = ((double)step) * twelfth + tint_radians;
        kern_i12[step] = -cos(phase);
        kern_q12[step] = sin(phase);
    }

    // Build I/Q decoder kernels
    for (int i=0; i<2*KCSIZE; i++) {
        double phase = ((double)i) * 2.0 * M_PI / 60.0 + tint_radians;
        kern_i[i] = -cos(phase);
        kern_q[i] =  sin(phase);
    }

    build_sinc_filter(kern_sinc_160, 769, 1.0 / 160.0);
    build_sinc_filter(kern_sinc_120, 513, 1.0 / 120.0);
    build_sinc_filter(kern_sinc_90,  385, 1.0 /  90.0);
    build_sinc_filter(kern_sinc_60,  257, 1.0 /  60.0);
    build_sinc_filter(bkern_sinc_90, 513, 1.0 /  90.0);
    
    // Generate a YIQ palette 
    for (int col=0; col<64; col++) {
        float *yiq = &yiq_table[col][0];
        int x = col & 15;        
        //double saturations[4] = { 0.260, 0.460, 0.400, 0.260 };
        //double intensities[4] = { 0.15, 0.6, 1.0, 1.2 };
        double saturations[4] = { 0.8, 0.65, 0.42, 0.26 };
        double saturation = saturations[col>>4];
        double intensities[4] = { 1.0, 1.0, 1.0, 1.2 };

        yiq[0] = 0.0;
        yiq[1] = 0.0;
        yiq[2] = 0.0;
        color_saturation[col] = 0.0;

        if (x < 0xE) {
            yiq[0] = (1.0-saturation) * intensities[col >> 4];            

            if (x == 0) {
                yiq[0] += 0.4;// * (1.0 - saturation);
            } else if (x == 0x0D) {
                yiq[0] -= intensities[0] * (1.0 - saturation);
            } else if (x < 0xD) {
                double phase = ((double)(x-1) + tint) * 2.0 * M_PI / 12.0;
                yiq[1] = saturation * -cos(phase);
                yiq[2] = saturation * sin(phase);
                color_saturation[col] = saturation;
                chroma_output[col][x-1] += saturation;
            }

        }
    }

    precompute_downsampling();

    vid_width = 640;
    vid_height = 480;
    vid_bpp = 32;
    filter_output_line = ntsc_emitter;
}



void yiq_test_emitter (unsigned y, byte *colors, byte *emphasis)
{
    Uint32 *dest0 = (Uint32 *) (((byte *)window_surface->pixels) + (y*2) * window_surface->pitch);
    Uint32 *dest1 = (Uint32 *) (((byte *)window_surface->pixels) + (y*2+1) * window_surface->pitch);
    
    for (int x = 0; x < 256; x++) {
        byte col = colors[x];
        Uint32 px;
        float rgb[3];
        //float yiq[3] = { 0.250*(col>>4), 0.0, 0.0 };

        yiq2rgb(yiq_table[col&63], rgb);
        px = rgbf(rgb[0], rgb[1], rgb[2]);
        

        *dest0++ = px; 
        *dest0++ = px;

        Uint32 nx = px;
        byte r = nx >> 16, g = (nx >> 8) & 0xFF, b = nx & 0xFF;
        r = r*2/4; 
        g = g*2/4; 
        b = b*2/4;
        nx = rgbi(r, g, b);

        dest1[1] = dest1[0] = nx;
        dest1 += 2;

    }
}
