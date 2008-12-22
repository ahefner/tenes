
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



static inline void render_tile (unsigned char *chrdata, tile image, unsigned char cols[4])
{
  unsigned char l;
  for (l = 0; l < 8; l++) {
    image[l * 8 + 7] = cols[(chrdata[l] & 1) + (2 * (chrdata[l + 8] & 1) >> 0)];
    image[l * 8 + 6] = cols[((chrdata[l] & 2) >> 1) + (2 * (chrdata[l + 8] & 2) >> 1)];
    image[l * 8 + 5] = cols[((chrdata[l] & 4) >> 2) + (2 * (chrdata[l + 8] & 4) >> 2)];
    image[l * 8 + 4] = cols[((chrdata[l] & 8) >> 3) + (2 * (chrdata[l + 8] & 8) >> 3)];
    image[l * 8 + 3] = cols[((chrdata[l] & 16) >> 4) + (2 * (chrdata[l + 8] & 16) >> 4)];
    image[l * 8 + 2] = cols[((chrdata[l] & 32) >> 5) + (2 * (chrdata[l + 8] & 32) >> 5)];
    image[l * 8 + 1] = cols[((chrdata[l] & 64) >> 6) + (2 * (chrdata[l + 8] & 64) >> 6)];
    image[l * 8 + 0] = cols[((chrdata[l] & 128) >> 7) + (2 * (chrdata[l + 8] & 128) >> 7)];
  }
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


void vid_draw_cache_page (int page, int x, int y, int pal)
{
  int i, j, k;
  SDL_LockSurface (surface);

  for (i = 0; i < 16; i++) {
    for (j = 0; j < 16; j++) {
      int idx = i * 16 + j;
      Uint32 *dst = (Uint32 *) (((unsigned char *) surface->pixels) + ((y + i * 8) * surface->pitch) + (x + j * 8));
      Uint32 *chr = (Uint32 *) tilecache[page][idx][pal];
      for (k = 0; k < 8; k++) {
	dst[0] = *chr;
	chr++;
	dst[1] = *chr;
	chr++;
	dst = (Uint32 *) (((unsigned char *) dst) + surface->pitch);
      }
    }
  }

  SDL_UnlockSurface (surface);
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

      attrdst[2] = ((*attrsrc) & 12) >> 2;
      attrdst[3] = ((*attrsrc) & 12) >> 2;
      attrdst[2 + 64] = ((*attrsrc) & 12) >> 2;
      attrdst[3 + 64] = ((*attrsrc) & 12) >> 2;

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
    attrdst += 32 + 192;
  }
}



void vid_build_tables (void)
{
  int x, y;
  for (y = 0; y < 60; y++) {
    for (x = 0; x < 64; x++) {
      tiletable[y][x] = nes.ppu.vram[((y < 30) ? 0x2000 : 0x2800) + y * 32 + (x & 31) + ((x < 32) ? 0 : 0x400)];
    }
  }
  
/*   memset((void *)attrtable,0,60*64*sizeof(int)); */
  vid_build_attr (&attrtable[0][0], nes.ppu.vram + 0x23C0);
  vid_build_attr (&attrtable[0][32], nes.ppu.vram + 0x27C0);
  vid_build_attr (&attrtable[30][0], nes.ppu.vram + 0x2BC0);
  vid_build_attr (&attrtable[30][32], nes.ppu.vram + 0x2FC0);
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
    int tcpage = (lineinfo[line].control1 & 0x10) >> 4;
    unsigned char *dest = (((unsigned char *)bytefb)+surface->pitch*line);
    int y = lineinfo[line].vscroll + line;
    int yoffset = y&7;
    int x = lineinfo[line].hscroll;
    int pixel=0;    
    int hscroll = lineinfo[line].hscroll;
    unsigned int *destl;
    unsigned char *data;
    int j;

    if (!(lineinfo[line].control2 & 8)) {
      memset ((void *)dest, nes.ppu.vram[0x3F00] ,256);
      continue;
    }
   
    if (lineinfo[line].control1 & 1) x+=256;
    if (lineinfo[line].control1 & 2) y+=240;      
    while (y>=480) y-=480;
    x>>=3;
    x&=63;
    y>>=3;
    
    //      printf ("%i%i\n", (int)nes.ppu.control1&2>>1, (int)nes.ppu.control1&1);
    
    if (hscroll&7) {
      data = tilecache[tcpage][tiletable[y][x]][attrtable[y][x]] + 8 * yoffset + (hscroll&7);
      for (j=hscroll&7; j<8; j++) *dest++ = *data++;
      //	pixel+=(scrollregs[line][0]&7);
      x = (x+1)&63;
    }
    
    destl = dest;
    for (j=0; j<31; j++) { 
      unsigned int *tdata = &tilecache[tcpage][tiletable[y][x]][attrtable[y][x]][8*yoffset];
      *destl++ = *tdata++;
      *destl++ = *tdata++;      
      x = (x+1)&63;
      //	pixel+=8;
    }      
    
    dest+=248;
    data = tilecache[tcpage][tiletable[y][x]][attrtable[y][x]] + 8*yoffset;
    for (j=0; j<((lineinfo[line].hscroll&7)?lineinfo[line].hscroll&7:8); j++) {
      *dest++ = *data++;
    }      
  }
      
  if (nes.ppu.control2 & 0x10) vid_draw_sprites (sx, sy);
  // else printf("sprites disabled.\n");
}

void vid_render_frame_old (int sx, int sy)
{
  unsigned hscroll = nes.ppu.hscroll;
  unsigned vscroll = nes.ppu.vscroll;
  unsigned char *bytefb = ((unsigned char *) surface->pixels) + sy * surface->pitch + sx;
  int x = hscroll >> 3, y = vscroll >> 3;
  int tcpage = (nes.ppu.control1 & 0x10) >> 4;
  int vtiles = 0;

  //  printf ("vid_render_frame: %i,%i\n", hscroll, vscroll);

  if (vid_tilecache_dirty) {
    gen_page (nes.ppu.vram, 0);
    gen_page (nes.ppu.vram + 0x1000, 1);
/*	printf("tilecache dirty.\n"); */
  }

/*   int ix,iy; */
  vid_build_tables ();

  if (vscroll & 7) {		/* top/bottom borders, yuck. */
    unsigned char *fbptr = bytefb;
    int i = 0;

    if (hscroll & 7) {		/* corner, double yuck. */
      vid_copytile_arbitrary (tilecache[tcpage][tiletable[y][x]]
			      [attrtable[y][x]] + (hscroll & 7) + 8 * (vscroll & 7), 8 - (hscroll & 7), 8 - (vscroll & 7), fbptr);
      x++;
      fbptr += (8 - (hscroll & 7));
      i = 1;
    }
    for (; i < 32; i++) {
      x &= 63;
      vid_copytile_hborder (tilecache[tcpage][tiletable[y][x]]
			    [attrtable[y][x]] + 8 * (vscroll & 7), 8 - (vscroll & 7), fbptr);
      fbptr += 8;
      x++;
    }
    if (hscroll & 7) {
      vid_copytile_arbitrary (tilecache[tcpage][tiletable[y][x]]
			      [attrtable[y][x]], hscroll & 7, 8 - (vscroll & 7), fbptr);
    }
    y++;  /* GNU indent was here. fear it. */
    if (y > 60)
      y -= 60;
    bytefb += (surface->pitch) * (8 - (vscroll & 7));
    vtiles++;
  }
  for (; vtiles < 30; vtiles++) {
    unsigned char *fbptr = bytefb;
    int i = 0;
    x = hscroll >> 3;

    if (hscroll & 7) {
      vid_copytile_arbitrary (tilecache[tcpage][tiletable[y][x]]
			      [attrtable[y][x]] + 8 - (hscroll & 7), 8 - (hscroll & 7), 8, fbptr);
      fbptr += (8 - (hscroll & 7));
      i = 0;
      x++;
    }
    for (; i < 32; i++) {
      x &= 63;
      vid_copytile_full (tilecache[tcpage][tiletable[y][x]]
			 [attrtable[y][x]], fbptr);
      fbptr += 8;
      x++;
    }
    if (hscroll & 7) {
      vid_copytile_arbitrary (tilecache[tcpage][tiletable[y][x]]
			      [attrtable[y][x]], hscroll & 7, 8, fbptr);
    }
    bytefb += 8 * surface->pitch;
    y++;
    if (y >= 60)
      y -= 60;
  }

  vid_draw_sprites (sx, sy);
}
