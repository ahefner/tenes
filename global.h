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
#define PBIT(n,datum)  ((1<<(n))&(datum))
#define GETBIT(n,datum)     (PBIT(n,datum)>>(n))
/* Creates a bitmask for nbits starting from the msb */
#define SUBMASK(msb,nbits) (((1<<(msb+1))-1)-((1<<(msb+1-nbits))-1))
#define LDB(msb,nbits,datum) (((datum)&SUBMASK(msb,nbits))>>(msb+1-nbits))
#define DPB(in,msb,nbits,datum) ((datum&(~SUBMASK(msb,nbits)))|(in<<(msb+1-nbits)))
#define KB(n) (n*1024)

#ifndef global_c
extern char *romfilename;
extern int sound_globalenabled;
extern int sound_muted;
extern int cputrace;
extern int forcemapper;
extern int cfg_trapbadops;
extern int vid_renderer;
extern int cfg_joythreshold;
extern int cfg_diagnostic;

extern int cfg_syncmode;
extern int cfg_framelines;
extern int cfg_vblanklines;
extern int cfg_linecycles;

/* Video/UI stuff */
extern int window_width;
extern int window_height;
extern int vid_fullscreen;
extern int tv_scanline;
extern SDL_Surface *window_surface; /* Window surface */

/* The renderer fills color_buffer at the beginning of the line, and
 * emphasis_bits are in filled by hblank as ppu.control2 changes.
 * These are the input to the video filter, which should combine them
 * to produce filtered RGB output.  An emphasis_position of -1
 * indicates that the emphasis is not being tracked presently (so that
 * we can know not to bother filling the emphasis buffer upon $2001
 * write during blank)
*/
extern byte color_buffer[256];
extern byte emphasis_buffer[256];
extern int emphasis_position;

/* Initialize the video filter (set output width/height, function pointers, ... */
extern void (*vid_filter) (void);
/* Output a scanline through the video filter */
extern void (*filter_output_line) (unsigned y, byte *colors, byte *emphasis);
extern int vid_width;           /* Filter output resolution, determining window surface */
extern int vid_height;
extern int vid_bpp;

extern int superverbose;
extern int trace_ppu_writes;
extern long long time_frame_start; /* microseconds */

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
