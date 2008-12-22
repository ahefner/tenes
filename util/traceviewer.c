/** cc `sdl-config --libs --cflags` traceviewer.c -o traceviewer && ./traceviewer  **/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "SDL.h"

#define max(x,y) ((x)>(y)?(x):(y))
#define min(x,y) ((x)>(y)?(y):(x))

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 1024

SDL_Surface *screen;

void init_pal (void);
void paint (void);

#define COUNT_READ 0
#define COUNT_WRITE 1
#define COUNT_EXEC 2

typedef unsigned char byte;

unsigned *counts;
byte *mem;

unsigned offset=0;

const int char_width=9, char_height=16;
SDL_Surface *font;
char *charmap="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()`~-_+=[]{};:'\",.<>/?\\|     ";

enum display_mode {normal, hexdump} display_mode;


int draw_char(int x, int y, char c, byte fg, byte bg)
{ 
  int width=(x+char_width > screen->w) ? screen->w-x : char_width;
  int height=(x+char_height > screen->h) ? screen->h-y : char_height;
  int idx=-1,i,len=strlen(charmap),cxidx,cyidx;
  byte *src;

  for (i=0; i<len; i++) {
    if (charmap[i]==c) { idx=i; break; }
  }

  if (idx==-1) return x+char_width;  
  cxidx=idx&63;
  cyidx=idx>>6;
  src=((byte *)font->pixels)+font->pitch*cyidx+char_width*cxidx;

  while (height--) {
    int x;
    byte *px=screen->pixels+screen->pitch*y+x;
    for (x=0; x<width; x++) {
      byte v=*src++;
      if (v) px[x]=(v==1?bg:fg);     
    }
    src=src+font->pitch-char_width;
    y++;
  }

  return x+char_width;
}

  
  


int draw_string (int x, int y, char *string)
{
  int i, len=strlen(string);  
  for (i=0; i<len; i++) {
    x+=draw_char(x,y,string[i], 4, 0);
  }  
  return x;
}

int main (int argc, char **argv)
{
  int i;
  Uint32 vid_extra_flags=0;
  SDL_Event event;
  FILE *trace,*dump;
  long length;

  font=SDL_LoadBMP("font-9x16.bmp");
  if (font==NULL) {
    printf("Could not open \"font-9x16.bmp\"!\n");
    return -1;
  }

  trace=fopen("trace-6502","rb");
  dump=fopen("dump-6502","rb");
  
  if ((!trace)||(!dump)) {
    printf("Could not open trace data!\n");
    if (trace) fclose(trace);
    if (dump) fclose(dump);
    return 1;
  }

  fseek(dump,0,SEEK_END);  /* stat is for chumps ;) */
  length=ftell(dump);
  fseek(dump,0,SEEK_SET);
  mem=malloc(length);
  counts=malloc(length*3*sizeof(unsigned));
  printf("Length is %u bytes, allocated at %p and %p\n",length,mem,counts);
  if (!(fread(mem,length,1,dump) && fread(counts,length*3*sizeof(unsigned),1,trace))) {
    printf("Trace data is inconsistant.\n");
    fclose(trace);
    fclose(dump);
    free(mem);
    free(counts);
    return 1;
  }

  for (i=1; i<argc; i++) {
    if (!strcasecmp(argv[i],"-fullscreen")) vid_extra_flags|=SDL_FULLSCREEN;
  }

  
  if (SDL_Init(SDL_INIT_VIDEO)<0) {
    fprintf(stderr,"Fuck!\n");
    return 1;
  }

  atexit(SDL_Quit);
  screen = SDL_SetVideoMode(1280,1024,8,SDL_SWSURFACE | vid_extra_flags);
  SDL_WM_SetCaption("Trace Viewer",NULL);

  init_pal();
  memset(screen->pixels,255,screen->w*screen->h);

  paint();
  SDL_UpdateRect(screen,0,0,1280,1024);
  
  while(1) {
    if (SDL_PollEvent(&event)) {
      if (event.type==SDL_VIDEOEXPOSE) {
	paint();
      }
	
      if ((event.type==SDL_QUIT)) {
	exit(0);
      }
    }
  }

  return 0;
}


#define pal_col(re,gr,bl) colors[idx].r=re; colors[idx].g=gr; colors[idx++].b=bl;
void init_pal (void)
{ 
  int idx=0;
  SDL_Color colors[256];
  memset(colors,0,sizeof(colors));
  pal_col(0,0,0);       /* 0 black */
  pal_col(0,0,255);     /* 1 blue */
  pal_col(255,0,0);     /* 2 red */
  pal_col(0,255,0);     /* 3 green */
  pal_col(255,255,255); /* 4 white */
  SDL_SetColors(screen,colors,0,256);
}

inline int trans (int count)
{
  if (!count) return 0;
  else if (count<0x80) return 1;
  else if (count<0x200) return 2;
  else if (count<0x8000) return 3;
  else if (count<0x20000) return 4;
  else return 5;
}

int bg_dirty=1;
byte data[SCREEN_WIDTH*SCREEN_HEIGHT]; /* a saved image of the trace data */

void paint_data (void);

void paint (void)
{
  int i,j;
  byte *px;

  SDL_LockSurface(screen);
  px=screen->pixels;

  if (bg_dirty) {
    paint_data();
    for (i=0; i<SCREEN_HEIGHT; i++) {
      memcpy(px+screen->pitch*i+screen->w-SCREEN_WIDTH,&data[SCREEN_WIDTH*i],SCREEN_WIDTH);
    }
    
    bg_dirty=0;
  }

  switch (display_mode) {
  case normal: break;
  case hexdump: break;
  }

  SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,1280,1024);
}

void paint_data (void)
{
  unsigned row,col,i=0,j;
  byte *px=data;
  memset(px,0,SCREEN_WIDTH*SCREEN_HEIGHT);

  for (col=0; col<64; col++) {
    for (row=0; row<1024; row++) {
      byte *dst=px+row*SCREEN_WIDTH+col*16;
      for (j=0; j<3; j++) {
	int k;
	int c=trans(counts[i*3+j]);
	for (k=0; k<c; k++) dst[k]=j+1;
	dst+=5;
      }
      *dst=4;
      i++;
    }
  }
}
