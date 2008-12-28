
#ifndef VID_H
#define VID_H

#include "sys.h"
#include "nes.h"

void render_clear (void);
void vid_drawpalette(int x,int y);
void vid_render_frame(int x,int y);

extern int vid_tilecache_dirty;

#endif
