#ifndef FILTERS_H
#define FILTERS_H

#include "global.h"

#ifndef FILTERS_C
/* The video stripe feature needs the RGB palette. */
extern unsigned rgb_palette[64];
extern unsigned grayscale_palette[64];
#endif

void rescale_2x (void);
void no_filter (void);
void scanline_filter (void);

#endif
