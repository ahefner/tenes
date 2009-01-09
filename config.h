#ifndef CONFIG_H
#define CONFIG_H

/* global configuration variables */
/* (why are these still here? These definitions live in global.h now... ) */

extern char *romfilename;
extern int sound_globalenabled;
extern int cputrace;
extern int cfg_trapbadops;
extern int forcemapper;
extern int vid_width;
extern int vid_height;
extern int vid_fullscreen;
extern int vid_renderer; /* 0=scanline, 1=simple */
extern int cfg_joythreshold;
extern int cfg_diagnostic;
extern int cfg_buttonmap[4][4];
extern int cfg_jsmap[4];
extern int cfg_disable_joysticks;

extern int cfg_framelines; 
extern int superverbose;

void cfg_parseargs(int argc,char **argv);

#endif
