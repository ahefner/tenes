#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sched.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#include "nespal.h"
#include "nes.h"
#include "config.h"

static long long tv_to_micros (struct timeval *tv)
{
    return (((long long)tv->tv_sec) * 1000000ll) + ((long long)tv->tv_usec);
}

long long usectime (void)
{
    struct timeval tv;
    if (gettimeofday(&tv, 0)) {
        perror("gettimeofday");
        exit(1);
    }
    return tv_to_micros(&tv);
}

void sys_framesync (void)
{
    long long target = time_frame_target;
    long long now;

    do {
        now = usectime();
        // FIXME: sys.c:44: sys_framesync: Assertion `(target - now) < 1000000ll' failed.
        //assert((target - now) < 1000000ll);
        if ((target-now)>1000000ll) {
            printf("?? %llu - %llu = %llu\n", target, now, target-now);
            return;
        }
        if (target-now > 10000) usleep(6000);
    } while (now < target);

    //if ((now-target)>1) printf("framesync: missed by %lli microseconds\n", now - target);
}

SDL_Color palette[129];
struct joystick joystick[4];
int numsticks = 0;
SDL_Window *window = NULL;

/* SDL3 dropped the old struct-of-masks pixel format (SDL_PixelFormat used
 * to *be* that struct; now it's just an enum tag like
 * SDL_PIXELFORMAT_XRGB8888). To keep swizzle_pixels() working across
 * whatever native format the window surface turns out to have, pull the
 * R/G/B masks out via SDL_GetMasksForPixelFormat() and count the shift
 * ourselves instead of relying on a field that no longer exists. */
static int shift_from_mask (Uint32 mask)
{
    int shift = 0;
    if (!mask) return 0;
    while (!(mask & 1)) { mask >>= 1; shift++; }
    return shift;
}

static void compute_rgb_shifts (SDL_PixelFormat format)
{
    int bpp;
    Uint32 rmask, gmask, bmask, amask;
    if (SDL_GetMasksForPixelFormat(format, &bpp, &rmask, &gmask, &bmask, &amask)) {
        rgb_shifts.r_shift = shift_from_mask(rmask);
        rgb_shifts.g_shift = shift_from_mask(gmask);
        rgb_shifts.b_shift = shift_from_mask(bmask);
    } else {
        /* Fallback: assume the extremely common XRGB8888 layout. */
        fprintf(stderr, "Warning: couldn't decode pixel format %s, assuming XRGB8888.\n",
                SDL_GetPixelFormatName(format));
        rgb_shifts.r_shift = 16;
        rgb_shifts.g_shift = 8;
        rgb_shifts.b_shift = 0;
    }
}

void print_video_info (void)
{
    printf("Video driver: %s\n", SDL_GetCurrentVideoDriver());
    printf("Window pixel format: %s   BPP: %i   R-shift:%i G-shift:%i B-shift:%i\n",
           SDL_GetPixelFormatName(window_surface->format),
           SDL_BITSPERPIXEL(window_surface->format),
           rgb_shifts.r_shift, rgb_shifts.g_shift, rgb_shifts.b_shift);
}

/* Re-fetch window_surface (and its shifts) from the SDL_Window. Needed
 * after sys_init()'s initial creation, and again any time something
 * could have invalidated the previous surface pointer -- resizing the
 * window or flipping fullscreen on/off. */
void sys_refresh_window_surface (void)
{
    window_surface = SDL_GetWindowSurface(window);
    if (window_surface == NULL) {
        printf("Could not get window surface: %s\n", SDL_GetError());
        exit(1);
    }
    compute_rgb_shifts(window_surface->format);
}

void sys_init (void)
{
    struct sched_param sparam;
    memset(&sparam, 0, sizeof(sparam));
    sparam.sched_priority = 1;
    // Probably a bad idea:
    //if (sched_setscheduler(getpid(), SCHED_FIFO, &sparam)) perror("sched_setscheduler");

    int i;
    SDL_WindowFlags winflags = vid_fullscreen ? SDL_WINDOW_FULLSCREEN : 0;

    if (!SDL_Init (SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        printf ("Could not initialize SDL! %s\n", SDL_GetError());
        exit (1);
    }

    /* Initializes video fitering */
    build_color_maps();
    if (vid_fullscreen && (vid_filter == no_filter)) vid_filter = rescale_2x;
    vid_filter();

    window_width = max(window_width, vid_width);
    window_height = max(window_height, vid_height);

    /* SDL3's SDL_CreateWindow() folds in what used to be SDL_SetVideoMode():
     * there's no separate bit-depth argument (you get a native-format,
     * CPU-addressable surface back from SDL_GetWindowSurface() below,
     * whatever bpp that turns out to be -- vid_bpp is no longer used to
     * request a mode) and no x/y position arguments (the window is placed
     * on the default display; use SDL_SetWindowPosition() if you ever
     * need to be picky about that). */
    window = SDL_CreateWindow("tenes", window_width, window_height, winflags);
    if (window == NULL) {
        printf("Could not create window: %s\n", SDL_GetError());
        exit(1);
    }

    sys_refresh_window_surface();
    print_video_info();

    SDL_SetWindowTitle(window, "tenes");
    SDL_FillSurfaceRect(window_surface, NULL, SDL_MapSurfaceRGB(window_surface, 0, 0, 0));
    SDL_UpdateWindowSurface(window);
    SDL_HideCursor();

    if (!cfg_disable_joysticks) {
        int njoy = 0;
        SDL_JoystickID *ids = SDL_GetJoysticks(&njoy);
        printf("Found %i joystick%s.\n", njoy, njoy==1?"":"s");
        if (njoy) {
            numsticks = (njoy>4)?4:njoy;
            for (i=0; i<numsticks; i++) {
                joystick[i].sdl = SDL_OpenJoystick(ids[i]);
                if (!joystick[i].sdl) printf ("Could not open joystick %i \n", i);
                else {
                    int j, js_mapping=-1;
                    joystick[i].connected = 1;
                    for (j=0; j<4; j++)
                    {
                        if (cfg_jsmap[j]==i)
                        {
                            js_mapping=j;
                            break;
                        }
                    }
                    printf ("  %i: %s (%i buttons)   ", i, SDL_GetJoystickName(joystick[i].sdl), SDL_GetNumJoystickButtons(joystick[i].sdl));
                    if (js_mapping==-1) printf("[unmapped]\n");
                    else printf("[joypad %i]\n", js_mapping);
                }
            }
            /* SDL3: joystick events are enabled per-joystick-subsystem via
             * this call rather than the old global SDL_JoystickEventState. */
            SDL_SetJoystickEventsEnabled(true);
        }
        if (ids) SDL_free(ids);
    } else printf("Joysticks are disabled.\n");
}



void sys_shutdown (void)
{
    int i;

    for (i=0; i<numsticks; i++) {
        if (joystick[i].sdl) SDL_CloseJoystick(joystick[i].sdl);
    }

    // SDL_DestroyWindow owns the surface and will free it.
    SDL_DestroyWindow(window);
    SDL_Quit ();
}

void image_free (image_t image)
{
    if (!image) return;
    SDL_DestroySurface(image->_sdl);
    if (image->freeptr) free(image->freeptr);
    free(image);
}

void make_dir (const char *path)
{
#ifdef _WIN32
    mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

void swizzle_pixels (uint32_t *pixels, size_t len)
{
    struct rgb_shifts sw = rgb_shifts;

    for (size_t x = 0; x < len; x++)
    {
        unsigned px = pixels[x];
        byte r = (px >> 16) & 0xFF, g = (px >> 8) & 0xFF, b = px & 0xFF;
        pixels[x] = (r << sw.r_shift) | (g << sw.g_shift) | (b << sw.b_shift);
    }
}
