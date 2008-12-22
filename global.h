#ifndef global_h
#define global_h

#include <SDL/SDL.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "nes.h"
#include "filters.h"
#include "M6502/M6502.h"

/* Normally instruction tracing should be off.. */
/* #define INSTRUCTION_TRACING */

#define BIT(x) (1<<(x))
#define PBIT(n,datum)  ((1<<(n))&datum)
#define GETBIT(n,datum)     (PBIT(n,datum)>>(n))
/* Creates a bitmask for nbits starting from the msb */
#define SUBMASK(msb,nbits) (((1<<(msb+1))-1)-((1<<(msb+1-nbits))-1))
#define LDB(msb,nbits,datum) (((datum)&SUBMASK(msb,nbits))>>(msb+1-nbits))
#define DPB(in,msb,nbits,datum) ((datum&(~SUBMASK(msb,nbits)))|(in<<(msb+1-nbits)))
#define KB(n) (n*1024)

#ifndef global_c
extern char *romfilename;
extern int sound_globalenabled;
extern int cputrace;
extern int forcemapper;
extern int cfg_trapbadops;
extern int vid_renderer;
extern int cfg_joythreshold;
extern int cfg_diagnostic;

extern struct nes_machine nes;

extern int cfg_syncmode;
extern int cfg_framelines;
extern int cfg_vblanklines;
extern int cfg_linecycles;

/* Video/UI stuff */
extern int window_width;
extern int window_height;
extern int vid_fullscreen;
extern int vid_width;
extern int vid_height;
extern SDL_Surface *window_surface; /* Window surface */
extern SDL_Surface *post_surface; /*  This is what is finally blitted to window_surface */
extern SDL_Surface *surface; /* Video output surface, possibly the same as the above two */

extern void (*vid_filter) (SDL_Surface *surface);

extern int superverbose;
extern struct timeval time_frame_start;

extern unsigned frame_number;

#ifdef INSTRUCTION_TRACING
#define COUNT_READ 0
#define COUNT_WRITE 1
#define COUNT_EXEC 2
extern unsigned tracing_counts[0x10000][3];
#endif

#endif


#define max(x,y) ((x)>(y)?(x):(y))
#define min(x,y) ((x)>(y)?(y):(x))

/* for byte and word data types */
#include "M6502/M6502.h"

/* ??? */
#define PrintBin(x) printf("%u%u%u%u%u%u%u%u",(unsigned)x>>7,(unsigned)(x>>6)&1,(unsigned)(x>>5)&1,(unsigned)(x>>4)&1,(unsigned)(x>>3)&1,(unsigned)(x>>2)&1,(unsigned)(x>>1)&1,(unsigned)x&1);

#endif 
