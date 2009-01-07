
#ifndef VID_H
#define VID_H

#include "sys.h"
#include "nes.h"

void render_clear (void);
void vid_drawpalette(int x,int y);
void render_scanline (void);
void catchup_emphasis (void);
void catchup_emphasis_to_x (int x);

#endif
