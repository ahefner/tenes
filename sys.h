#ifndef NES_SYS_H
#define NES_SYS_H

#include <SDL/SDL.h>

extern volatile unsigned long heartbeat;
extern SDL_Surface *surface;
extern SDL_Joystick *joystick[4];
extern int numsticks;

void sys_init(void);
void sys_shutdown(void);
void sys_framesync (void);
char *ensure_config_dir (void);
char *ensure_save_dir (void);
long long usectime (void);


#endif
