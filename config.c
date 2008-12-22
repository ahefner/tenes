#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include "global.h"


void cfg_parseargs (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++) {
    char *txt = argv[i];
    if (txt[0] == '-') {
      if (!strcmp(txt, "-nosound"))
	sound_globalenabled = 0;
      if (!strcmp(txt, "-sound"))
	sound_globalenabled = 1;
      if (!strcmp(txt, "-width")) {
	if (i != (argc - 1)) {
	  i++;
	  window_width = atoi (argv[i]);
	} else {
	  printf ("-width requires a numeric value.\n");
	}
      }
      if (!strcmp (txt, "-height")) {
	if (i != (argc - 1)) {
	  i++;
	  window_height = atoi (argv[i]);
	} else {
	  printf ("-height requires a numeric value.\n");
	}
      }
      if (!strcmp(txt, "-mapper")) {
	if (i != (argc - 1)) {
	  i++;
	  forcemapper = atoi (argv[i]);
	} else {
	  printf ("-mapper requires a numeric value.\n");
	}
      }
      if (!strcmp (txt, "-fullscreen"))
	vid_fullscreen = 1;
      if (!strcmp (txt, "-r_scanline"))
	vid_renderer = 0;
      else if (!strcmp (txt, "-r_simple"))
	vid_renderer = 1;
      if (!strcmp (txt, "-windowed"))
	vid_fullscreen = 0;
      if (!strcmp (txt, "-cputrace"))
	cputrace = 1;
      if (!strcmp (txt, "-trapbadops")) 
	cfg_trapbadops = 1;      
      if (!strcmp (txt, "-forcesram"))
	nes.rom.flags|=2;
      if (!strcmp (txt, "-diagnostic")) {
	cfg_diagnostic = 1;
	window_width = 640;
	window_height = 480;
      }

      if (!strcmp(txt, "-superverbose")) superverbose = 1;

      if (!strcmp(txt, "-despair")) cfg_disable_joysticks = 1;
      if (!strcmp(txt, "-jsmap")) {
	if (argc<=(++i)) break;
	sscanf (argv[i],"%i,%i,%i,%i", &cfg_jsmap[0], &cfg_jsmap[1], &cfg_jsmap[2], &cfg_jsmap[3]);
      }   
      if (!strcmp(txt, "-joy0")) {
	if (argc<=(++i)) break;
	sscanf (argv[i],"%i,%i,%i,%i", &cfg_buttonmap[0][0], &cfg_buttonmap[0][1], &cfg_buttonmap[0][2], &cfg_buttonmap[0][3]);
      } 
      if (!strcmp(txt, "-joy1")) {
	if (argc<=(++i)) break;
	sscanf (argv[i],"%i,%i,%i,%i", &cfg_buttonmap[1][0], &cfg_buttonmap[1][1], &cfg_buttonmap[1][2], &cfg_buttonmap[1][3]);
      }
      if (!strcmp(txt, "-joy2")) {
	if (argc<=(++i)) break;
	sscanf (argv[i],"%i,%i,%i,%i", &cfg_buttonmap[2][0], &cfg_buttonmap[2][1], &cfg_buttonmap[2][2], &cfg_buttonmap[2][3]);
      }
      if (!strcmp(txt, "-joy3")) {
	if (argc<=(++i)) break;
	sscanf (argv[i],"%i,%i,%i,%i", &cfg_buttonmap[3][0], &cfg_buttonmap[3][1], &cfg_buttonmap[3][2], &cfg_buttonmap[3][3]);
      }
      if (!strcmp(txt, "-scale")) {
	vid_filter=rescale_2x;
      }
/*      
      if (!strcmp(txt, "-filter")) {
	vid_filter=rescale_filtered;
	printf("Using Scale2x pixel filter, by Andrea Mazzoleni.\n");
      }      
*/

    } else {
      romfilename = txt;
    }
  }
}
