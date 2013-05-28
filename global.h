#ifndef global_h
#define global_h

#include <SDL/SDL.h>
#include <sys/time.h>
#include <time.h>

#include "nes.h"
#include "config.h"
#include "filters.h"
#include "utility.h"
#include "ui.h"
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

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

/* I'm really starting to hate GCC. */
#define noinline __attribute__((noinline))

#ifndef global_c
extern char *romfilename;
extern int running;
extern int sound_globalenabled;
extern int sound_muted;
extern int cputrace;
extern int debug_brk;
extern int forcemapper;
extern int cfg_trapbadops;
extern int cfg_diagnostic;
extern int no_throttle;
extern int startup_restore_state;

extern int cfg_joythreshold;
extern int cfg_buttonmap[4][4];
extern int cfg_jsmap[4];
extern int cfg_keyboard_controller;
extern SDLKey keymap[8];
extern int cfg_disable_joysticks;
extern int cfg_disable_keyboard;

extern const char *movie_output_filename;
extern const char *movie_input_filename;
extern FILE *movie_output, *movie_input;
extern int quit_after_playback;

extern FILE *video_stripe_output;
extern byte video_stripe_idx;
extern unsigned video_stripe_rate;

extern int cfg_syncmode;
extern int cfg_framelines;
extern int video_alignment_cycles_kludge;

extern unsigned sprite0_hit_cycle; /* Cycle at which first sprite0 in current line occured */
extern unsigned sprite0_detected;  /* Was sprite0 hit detected during rendering? */

extern byte ram32k[0x8000];

extern int nsf_seek_to_song;

extern int aux_axis[2];
extern float aux_position[2];

/* Video/UI stuff */
extern int window_width;
extern int window_height;
extern int vid_fullscreen;
extern int tv_scanline;
extern int rendering_scanline;
extern SDL_Surface *window_surface; /* Window surface */

extern struct rgb_shifts rgb_shifts;


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

extern byte frame_buffer[2][SCREEN_HEIGHT][SCREEN_WIDTH];

/* Initialize the video filter (set output width/height, function pointers, ... */
extern void (*vid_filter) (void);
/* Output a scanline through the video filter */
extern void (*filter_output_line) (unsigned y, byte *colors, byte *emphasis);
extern void (*filter_output_finish) (void);

extern int vid_width;           /* Filter output resolution, determining window surface */
extern int vid_height;
extern int vid_bpp;

extern int superverbose;
extern int trace_mem_writes;
extern int trace_ppu_writes;
extern long long time_frame_start; /* microseconds */
extern long long time_frame_target;
extern unsigned frame_start_samples;

extern int unique_frame_number;

extern const char *cfg_mountpoint;
extern int cfg_mount_fs;

extern SDL_mutex *producer_mutex;

#ifdef INSTRUCTION_TRACING
#define COUNT_READ 0
#define COUNT_WRITE 1
#define COUNT_EXEC 2
extern unsigned tracing_counts[0x10000][3];
#endif

static inline Uint32 *display_ptr (int x, int y)
{
    return (Uint32 *) (((byte *)window_surface->pixels) + y * window_surface->pitch + x * 4);
}

static inline Uint32 *next_line (Uint32 *ptr, int n)
{
    return (Uint32 *)(((byte *)ptr) + n*window_surface->pitch);
}

#endif

#define max(x,y) ((x)>(y)?(x):(y))
#define min(x,y) ((x)>(y)?(y):(x))

/* for byte and word data types */
#include "M6502/M6502.h"

#endif
