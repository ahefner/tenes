
#ifndef VID_H
#define VID_H

#include "sys.h"
#include "nes.h"

void vid_drawpalette(int x,int y);
void gen_page(unsigned char *patterns,int pageidx);
void vid_draw_cache_page (int page,int x,int y,int pal);
void vid_render_frame(int x,int y);

extern int vid_tilecache_dirty;

#endif
