#ifndef FILTERS_H
#define FILTERS_H

#include "global.h"

#ifndef FILTERS_C
/* The video stripe feature needs the RGB palette. */
extern unsigned rgb_palette[64];
extern unsigned grayscale_palette[64];
#endif

void ntsc_filter (void);
void rescale_2x (void);
void no_filter (void);
void scanline_filter (void);
void filter_finish_nop (void);

void viewhash_filter (void);
void viewhash_display (void);

#endif
