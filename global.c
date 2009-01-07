#define global_c

#include "nes.h"


char *romfilename = "";
int sound_globalenabled = 1;
int sound_muted = 0;
int cputrace = 0;
int forcemapper = -1;
int cfg_trapbadops = 0;
int cfg_joythreshold = 4096;
int superverbose = 0;
int trace_ppu_writes = 0;
int cfg_diagnostic = 0;
int cfg_disable_joysticks = 0;

int window_width = 256;
int window_height = 256;
int vid_fullscreen = 0;

int tv_scanline = 0;

SDL_Surface *window_surface=NULL;

void (*vid_filter) (void) = rescale_2x;
void (*filter_output_line) (unsigned y, byte *colors, byte *emphasis) = NULL;
int vid_width = 256;  /* These may increse if pixel filtering is enabled */
int vid_height = 240; /* "                                             " */
int vid_bpp = 32;

byte color_buffer[256];
byte emphasis_buffer[256];
int emphasis_position = -1;

int cfg_buttonmap[4][4] = { {2,1,4,5}, {2,1,4,5}, {2,1,4,5}, {2,1,4,5}};
int cfg_jsmap[4] = {0,1,2,3};

long long time_frame_start;
long long time_frame_target;
unsigned frame_start_samples = 0;

/* timing config - move inside nes_machine if PAL support is added */
int cfg_framelines = 240; 
int cfg_vblanklines = 20;
int cfg_linecycles = 114;

#ifdef INSTRUCTION_TRACING
unsigned tracing_counts[0x10000][3];
#endif

/* TODO: I need a way to distinguish even/odd frames. I'll use this,
 * so it should go into the nes struct, for the sake of the save
 * file. */
unsigned frame_number = 0;
