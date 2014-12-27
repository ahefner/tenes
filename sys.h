#ifndef NES_SYS_H
#define NES_SYS_H

#include <SDL.h>

// Stupid image wrapper, because you really need alignment offsets
// attached to the image to have a nice interface. Also helps with
// memory management.

struct myimage {
    int w, h, x_origin, y_origin;
    SDL_Surface *_sdl;
    void *freeptr;
};
typedef struct myimage *image_t;

void image_free (image_t image);

struct joystick
{
    int connected;
    SDL_Joystick *sdl;
    image_t title, maker, description;
};

extern volatile unsigned long heartbeat;
extern SDL_Surface *surface;
//extern SDL_Joystick *joystick[4];
extern struct joystick joystick[4];
extern int numsticks;

void sys_init(void);
void sys_shutdown(void);
void sys_framesync (void);
long long usectime (void);
void make_dir (const char *path);

#endif
