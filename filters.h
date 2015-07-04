#ifndef FILTERS_H
#define FILTERS_H

#include "global.h"

#ifndef FILTERS_C
/* The video stripe feature needs the RGB palette. */
extern unsigned rgb_palette[64];
extern unsigned grayscale_palette[64];
#endif

void build_color_maps (void);

void ntsc_filter (void);
void ntsc2x_filter (void);
void rescale_2x (void);
void no_filter (void);
void scanline_filter (void);
void filter_finish_nop (void);

void viewhash_filter (void);
void viewhash_display (void);

void emit_unscaled (Uint32 *out, byte *colors, byte *emphasis, size_t n);

static inline Uint32 rgbi (byte r, byte g, byte b)
{
    return (r << 16) | (g << 8) | b;
}

static inline byte clamp_byte (int x)
{
    if (x < 0) return 0;
    if (x > 255) return 255;
    else return x;
}

static inline Uint32 rgbi_clamp (int r, int g, int b)
{
    return rgbi(clamp_byte(r), clamp_byte(g), clamp_byte(b));
}

#endif
