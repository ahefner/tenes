
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "sys.h"
#include "nes.h"
#include "global.h"

int vid_tilecache_dirty = 1;

void vid_drawpalette (int x, int y)
{
    int j;
    for (j = 0; j < 16; j++) {
        SDL_Rect r = { x + j * 16, y, 16, 16 };
        SDL_FillRect (surface, &r, nes.ppu.vram[0x3F00 + j] & 0x3F);
    }
    for (j = 0; j < 16; j++) {
        SDL_Rect r = { x + j * 16, y + 16, 16, 16 };
        SDL_FillRect (surface, &r, nes.ppu.vram[0x3F10 + j] & 0x3F);
    }
}

/* really disturbing tile cache stuff */

typedef unsigned char tile[64];
typedef tile tile_group[9];
typedef tile_group tc_page[256];

tc_page tilecache[2];

static inline void unpack_chr_byte (byte line[8], unsigned char *chrdata)
{
    byte low = chrdata[0], high = chrdata[8];
    line[7] = ((low &   1) >> 0) | ((high & 1)   << 1);
    line[6] = ((low &   2) >> 1) | ((high & 2)   >> 0);
    line[5] = ((low &   4) >> 2) | ((high & 4)   >> 1);
    line[4] = ((low &   8) >> 3) | ((high & 8)   >> 2);
    line[3] = ((low &  16) >> 4) | ((high & 16)  >> 3);
    line[2] = ((low &  32) >> 5) | ((high & 32)  >> 4);
    line[1] = ((low &  64) >> 6) | ((high & 64)  >> 5);
    line[0] = ((low & 128) >> 7) | ((high & 128) >> 6);
}

void render_tile (byte *chrdata, tile image, unsigned char cols[4])
{
    unsigned char l, i;
    for (l = 0; l < 8; l++) {
        unpack_chr_byte(&image[l*8], chrdata + l);
        for (i=0; i<8; i++) image[l*8+i] = cols[image[l*8+i]];
    }
}

static inline void unpack_tile (byte out[8], int page, byte tileindex, byte y_offset)
{
    word addr = page * 0x1000 + tileindex * 16 + y_offset;
    unpack_chr_byte(out, nes.ppu.vram + addr);
}

/* builds a cache group for one character */
void gen_group (unsigned char *chrdata, tile_group grp)
{ 
  unsigned char cols[4];
  int i, j;

  cols[0] = 0;
  cols[1] = 1;
  cols[2] = 2;
  cols[3] = 3;
  render_tile (chrdata, grp[8], cols);

  for (i = 0; i < 8; i++) {
    for (j = 1; j < 4; j++) cols[j] = nes.ppu.vram[0x3F00 + i * 4 + j];     
    cols[0] = nes.ppu.vram[0x3F00];
    for (j=0; j<64; j++) {
      grp[i][j] = cols[grp[8][j]];
      if (grp[8][j]) grp[i][j]|=64;
    }
  }
}

/* builds a tile cache page for a full pattern table */
void gen_page (unsigned char *patterns, int pageidx)
{
  int i;

  for (i = 0; i < 256; i++) {
    gen_group (patterns + i * 16, tilecache[pageidx][i]);
  }
}

void vid_copytile_arbitrary (unsigned char *tile, int width, int height, unsigned char *dst)
{
  int w, h;
  for (h = 0; h < height; h++) {
    for (w = 0; w < width; w++) {
      dst[w] = tile[w];
    }
    dst += surface->pitch;
    tile += 8;
  }
}

void vid_copytile_hborder (unsigned char *tile, int height, unsigned char *dst)
{
  Uint32 *t = (Uint32 *) tile;
  int y;
  for (y = 0; y < height; y++) {
    Uint32 *dst32 = (Uint32 *) dst;
    dst32[0] = t[0];
    dst32[1] = t[1];
    dst += surface->pitch;
    t += 2;
  }
}

static inline void vid_copytile_full (unsigned char *tile, unsigned char *dst)
{
  Uint32 *t = (Uint32 *) tile;
  Uint32 *dst32 = (Uint32 *) dst;
  unsigned int wrap = surface->pitch - 8, i;

  for (i = 0; i < 8; i++) {
    *dst32++ = *t++;
    *dst32++ = *t++;
    dst32 = (Uint32 *) (((unsigned char *) dst32) + wrap);
  }
}

int tiletable[60][64];
int attrtable[60][64];

void vid_build_attr (int *attrdst, unsigned char *attrsrc)
{
  int x, y;
  for (y = 0; y < 8; y++) {
    for (x = 0; x < 8; x++) {
      attrdst[0] = (*attrsrc) & 3;
      attrdst[1] = (*attrsrc) & 3;
      attrdst[0 + 64] = (*attrsrc) & 3;
      attrdst[1 + 64] = (*attrsrc) & 3;

      attrdst[2] = ((*attrsrc) >> 2) & 3;
      attrdst[3] = ((*attrsrc) >> 2) & 3;
      attrdst[2 + 64] = ((*attrsrc) >> 2) & 3;
      attrdst[3 + 64] = ((*attrsrc) >> 2) & 3;

      if (y < 7) {
	attrdst[0 + 128] = ((*attrsrc) & 0x30) >> 4;
	attrdst[1 + 128] = ((*attrsrc) & 0x30) >> 4;
	attrdst[0 + 192] = ((*attrsrc) & 0x30) >> 4;
	attrdst[1 + 192] = ((*attrsrc) & 0x30) >> 4;

	attrdst[2 + 128] = ((*attrsrc) & 0xC0) >> 6;
	attrdst[3 + 128] = ((*attrsrc) & 0xC0) >> 6;
	attrdst[2 + 192] = ((*attrsrc) & 0xC0) >> 6;
	attrdst[3 + 192] = ((*attrsrc) & 0xC0) >> 6;
      }
      attrdst += 4;
      attrsrc++;
    }
    attrdst += (32 + 192);
  }
}



void vid_build_tables (void)
{
  int x, y;
  for (y = 0; y < 60; y++) {
    for (x = 0; x < 64; x++) {
        word addr = ((y < 30) ? 0x2000 + y*32 : 0x2800 + (y-30)*32) 
                    + (x & 31) + ((x < 32) ? 0 : 0x400);
        tiletable[y][x] = nes.ppu.vram[ppu_mirrored_nt_addr(addr)];
    }
  }
  
/*   memset((void *)attrtable,0,60*64*sizeof(int)); */
  vid_build_attr (&attrtable[0][0], nes.ppu.vram + ppu_mirrored_nt_addr(0x23C0));
  vid_build_attr (&attrtable[0][32], nes.ppu.vram + ppu_mirrored_nt_addr(0x27C0));
  vid_build_attr (&attrtable[30][0], nes.ppu.vram + ppu_mirrored_nt_addr(0x2BC0));
  vid_build_attr (&attrtable[30][32], nes.ppu.vram + ppu_mirrored_nt_addr(0x2FC0));
}


/*void vid_draw_sprites (int sx, int sy)
{
  int i;
  int tcpage = (nes.ppu.control1 & 8) >> 3;
  int soffset = tilecache[(~tcpage)&1]-tilecache[tcpage];
  
  for (i = 0; i < 64; i++) {
    unsigned char *src = tilecache[tcpage][nes.ppu.spriteram[i * 4 + 1]][(nes.ppu.spriteram[i * 4 + 2] & 3) + 4];
    unsigned char *tsrc = tilecache[tcpage][nes.ppu.spriteram[i * 4 + 1]][8];
    unsigned char *dst = ((unsigned char *) surface->pixels) + surface->pitch * (sy + nes.ppu.spriteram[i * 4])
      + nes.ppu.spriteram[i * 4 + 3] + sx;
    int dx = (nes.ppu.spriteram[i * 4 + 2] & 0x40) ? -1 : 1;
    int dy = (nes.ppu.spriteram[i * 4 + 2] & 0x80) ? -1 : 1;
    int y = (dy == 1) ? 0 : 7;
    int x0 = (dx == 1) ? 0 : 7;
    int j, k;

    if (nes.ppu.spriteram[i * 4] > 224)
      continue;


    for (j = 0; j < 8; j++) {
      unsigned char *fb = dst;
      int x = x0;
      for (k = 0; k < 8; k++) {
	if (tsrc[y * 8 + x])
	  fb[k] = src[y * 8 + x];
	x += dx;
      }
      y += dy;
      dst += surface->pitch;
    }
    
    if (nes.ppu.control1 & 0x20) {
      y=(dy == 1) ? 0 : 7;
      src+=soffset;
      tsrc+=soffset;
      for (j = 0; j < 8; j++) {
	unsigned char *fb = dst;
	int x = x0;
	for (k = 0; k < 8; k++) {
	  if (tsrc[y * 8 + x])
	    fb[k] = src[y * 8 + x];
	  x += dx;
	}
	y += dy;
	dst += surface->pitch;
      }
    }
    
  }
} */



void vid_draw_sprite_tile (int sx, int sy, int page, int tile, int color, int hflip, int vflip, int behind)
{
  byte *src = tilecache[page][tile][color];
  byte *tsrc = tilecache[page][tile][8];  
  byte *dest = ((unsigned char *)surface->pixels) + sx + sy*surface->pitch;
  int x,y;
  switch (hflip) {
  case 0:
    switch (vflip) {
    case 0:
      for (y=0; y<8; y++) {
	for (x=0; x<8; x++) { 
	    if (*tsrc++) if ((!behind) || (!(dest[x]&64))) dest[x]=src[x]; 
	  }
	dest+=surface->pitch;
	src+=8;
      }
      break;
    case 1:
      for (y=7; y>=0; y--) {
	byte *d = dest + y*surface->pitch;
	for (x=0; x<8; x++) { if (*tsrc++) if ((!behind) || (!(d[x]&64))) d[x]=src[x]; }
	src+=8;
      }
      break;
    }
    break;
  case 1:
    switch (vflip) {
    case 0:
      for (y=0; y<8; y++) {
	for (x=7; x>=0; x--)
	  if (tsrc[x]) if ((!behind) || (!(dest[(~x)&7]&64))) dest[(~x)&7]=src[x];
	dest+=surface->pitch;
	tsrc+=8;
	src+=8;
      }
      break;
    case 1:
      for (y=7; y>=0; y--) {
	byte *d = dest + y*surface->pitch;	
	for (x=7; x>=0; x--) if (tsrc[x]) if ((!behind) || (!(d[(~x)&7]&64))) d[(~x)&7]=src[x];
	src+=8;
	tsrc+=8;
      }
      break;
    }
    break;
  }
}


void vid_draw_sprites (int sx, int sy)
{
  int i;

  /*  if (nes.ppu.control1 & 0x20) printf ("Using tall sprites!!\n"); */

  for (i=63; i>=0; i--) {
    int tile = nes.ppu.spriteram[i*4+1];
    int color = (nes.ppu.spriteram[i*4+2] & 3) + 4;
    int x = nes.ppu.spriteram[i*4+3];
    int y = nes.ppu.spriteram[i*4];
    int tcpage = (lineinfo[y].control1 & 8) >> 3;
    int hflip = (nes.ppu.spriteram[i*4+2] & 0x40) >> 6;
    int vflip = (nes.ppu.spriteram[i*4+2] & 0x80) >> 7;
    int priority = (nes.ppu.spriteram[i*4+2] & 0x20) >> 5;

    if (y>224) continue;
  
    /* 8x16 sprites? */
    if (nes.ppu.control1 & 0x20) {
      int page0 = tile&1, page1 = page0;
      int tile0 = tile&0xFE;
      int tile1 = tile0 + 1;     
      /* int page0 = 0, page1 = 1;   */
      /* int page0 = tcpage, page1 = (~tcpage)&1; */
      if (vflip) {
	vid_draw_sprite_tile (sx+x, sy+y, page1 , tile1, color, hflip, 1, priority);
	vid_draw_sprite_tile (sx+x, sy+y+8, page0, tile0, color, hflip, 1, priority);
      } else {
	vid_draw_sprite_tile (sx+x, sy+y, page0, tile0, color, hflip, 0, priority);
	vid_draw_sprite_tile (sx+x, sy+y+8, page1, tile1, color, hflip, 0, priority);
      }
    } else { 
      /* normal 8x8 sprite */
      vid_draw_sprite_tile (sx+x, sy+y, tcpage, tile, color, hflip, vflip, priority);
    }
  }
}




/*
struct chr_shifter {
    byte low;
    byte high;
};

struct sprite_unit {
    struct chr_shifter shifter;
    int palette;
};
*/



void vid_render_frame (int sx, int sy)
{
  unsigned char *bytefb = ((unsigned char *) surface->pixels) + sy * surface->pitch + sx;
  int line;

  vid_build_tables ();
  if (vid_tilecache_dirty) {
    gen_page (nes.ppu.vram, 0);
    gen_page (nes.ppu.vram + 0x1000, 1);
  }

  
  for (line=0; line<240; line++) {
    int xoff = lineinfo[line].x;
    int v = lineinfo[line].v;
    int control1 = lineinfo[line].control1;
    int control2 = lineinfo[line].control2;

    /* A future reworking of the code will render from the V register directly. */
    int hscroll = xoff + ((v & 0x1F) << 3);
    int vscroll = (((v >> 5)&0x1F)<<3) + (v >> 12);

    int tcpage = (control1 & 0x10) >> 4;
    unsigned char *dest = (((unsigned char *)bytefb)+surface->pitch*line);
    int y = vscroll + line;
    int yoffset = y&7;
    int x = hscroll;

    unsigned int *destl;
//    unsigned char data[8];
    unsigned char *data;
    int j;

    if (!(control2 & 8)) {
      memset ((void *)dest, nes.ppu.vram[0x3F00], 256);
      continue;
    }
   
    /* if (control1 & 1) x += 256;
       if (control1 & 2) y += 240; */

    if (v & 0x400) x+= 256;
    if (v & 0x800) y+= 240;
    while (y >= 480) y -= 480;
    y >>= 3;
    x >>= 3;
    x &= 63;

    
    //      printf ("%i%i\n", (int)nes.ppu.control1&2>>1, (int)nes.ppu.control1&1);
    
    if (xoff) {
        byte *d;
        d = tilecache[tcpage][tiletable[y][x]][attrtable[y][x]] + 8 * yoffset + (xoff&7);
        //tileseg(data, tcpage, tiletable[y][x], attrtable[y][x], yoffset);
        //d += xoff & 7;
        for (j=xoff&7; j<8; j++) *dest++ = *d++;
        x = (x+1)&63;
    }
    
    destl = dest;
    for (j=0; j<31; j++) { 
        unsigned int *tdata = &tilecache[tcpage][tiletable[y][x]][attrtable[y][x]][8*yoffset];
        //unsigned int tile[2];
        //unpack_tile((byte *)tile, tcpage, tiletable[y][x], y_offset);
        
        *destl++ = *tdata++;
        *destl++ = *tdata++;      
        x = (x+1)&63;
      //	pixel+=8;
    }      
    
    dest+=248;
    data = tilecache[tcpage][tiletable[y][x]][attrtable[y][x]] + 8*yoffset;
    for (j=0; j<(xoff?xoff:8); j++) {
      *dest++ = *data++;
    }
  }
      
  if (nes.ppu.control2 & 0x10) vid_draw_sprites (sx, sy);
}


