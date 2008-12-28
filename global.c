#define global_c

#include "nes.h"


char *romfilename = "smb1.nes";
int sound_globalenabled = 1;
int cputrace = 0;
int forcemapper = -1;
int cfg_trapbadops = 0;
int vid_renderer = 0;
int cfg_joythreshold = 4096;
int superverbose = 0;
int trace_ppu_writes = 0;
int cfg_diagnostic = 0;
int cfg_disable_joysticks = 0;

int window_width = 256;
int window_height = 256;
int vid_fullscreen = 0;

int vid_width = 256;  /* These may increse if pixel filtering is enabled */
int vid_height = 240; /* "                                             " */

int tv_scanline = 0;

SDL_Surface *surface=NULL, *window_surface=NULL, *post_surface=NULL;
void (*vid_filter) (SDL_Surface *surface)=NULL;

int cfg_buttonmap[4][4] = { {2,1,4,5}, {2,1,4,5}, {2,1,4,5}, {2,1,4,5}};
int cfg_jsmap[4] = {0,1,2,3};

//struct nes_machine nes;
struct scanline_info lineinfo[LOG_LENGTH];

struct timeval time_frame_start;

/* timing config - move inside nes_machine if PAL support is added */
int cfg_framelines = 240; 
int cfg_vblanklines = 23;
int cfg_linecycles = 114;

#ifdef INSTRUCTION_TRACING
unsigned tracing_counts[0x10000][3];
#endif

unsigned frame_number = 0;
