#define global_c

#include "nes.h"

/* An impressive collection of global state.
   Most of this is read-only configuration options. */

char *romfilename = "";
int running = 1;
int sound_globalenabled = 1;
int sound_muted = 0;            /* Mutable (hah!) */
int debug_brk = 0;
int forcemapper = -1;
int cfg_trapbadops = 0;
int no_throttle = 0;
int startup_restore_state = 0;

int cputrace = 0;               /* These tracing options are all togglable */
int superverbose = 0;
int trace_ppu_writes = 0;
int cfg_diagnostic = 0;

int cfg_buttonmap[4][4] = { {2,1,4,5}, {2,1,4,5}, {2,1,4,5}, {2,1,4,5}};
int cfg_jsmap[4] = {0,1,2,3};
int aux_axis[2] = {0,0};
int cfg_joythreshold = 4096;
int cfg_disable_joysticks = 0;
int cfg_disable_keyboard = 0;

float aux_position[2] = {0,0};  /* Mutable */
int mouse_x = - 1, mouse_y = -1; /* Mouse state is mutable */
int mouse_clicked = 0;
int mouse_pressed = 0;

char *movie_output_filename = NULL;
char *movie_input_filename = NULL;
FILE *movie_output = NULL, *movie_input = NULL; /* Mutable */
int quit_after_playback = 0;

FILE *video_stripe_output = NULL;
byte video_stripe_idx = 128;
unsigned video_stripe_rate = 0;

int window_width = 256;
int window_height = 256;
int vid_fullscreen = 0;         /* Mutable */

int tv_scanline = 0;            /* Mutable */

/* Set during first 256 cycles of scanline, but not during hblank: */
int rendering_scanline = 0;

SDL_Surface *window_surface=NULL;

void (*vid_filter) (void) = rescale_2x;
void (*filter_output_line) (unsigned y, byte *colors, byte *emphasis) = NULL;
void (*filter_output_finish) (void) = filter_finish_nop;

int vid_width = 256;  /* These may increse if pixel filtering is enabled */
int vid_height = 240; /* "                                             " */
int vid_bpp = 32;

byte color_buffer[256];
byte emphasis_buffer[256];
int emphasis_position = -1;

long long time_frame_start;     /* More mutable stuff, for sys_framesync */
long long time_frame_target;
unsigned frame_start_samples = 0;

/* timing config - move inside nes_machine if PAL support is added */
int cfg_framelines = 240; 

/* Kludge pixel offset applied to $2001 writes, to calibrate the Final Fantasy light beam effect. */
int video_alignment_cycles_kludge = 12;

#ifdef INSTRUCTION_TRACING
unsigned tracing_counts[0x10000][3];
#endif

/* There's two competing notions of "frame number" at play. This one
 * is guaranteed to increment between every frame of emulation,
 * uninterrupted by restoring state, so that each frame can be
 * uniquely identified. The other is the 'time' member of the NES
 * struct, which counts up from 0 and is saved and restored as part of
 * the state. These two can diverge when a state restore occurs.
 */
int unique_frame_number = 0;

SDL_mutex *producer_mutex;      /* Held while filling audio buffer */

char *cfg_mountpoint = "/tmp/nesfs";
int cfg_mount_fs = 0;

