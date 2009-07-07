#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"

void print_usage (void)
{
    printf("Usage: nesemu [OPTION]... ROMFILE\n"
           "Emulate an NTSC NES running ROMFILE (in iNES .nes format)\n"
           "\n"
           "Options:\n"
           " -help           Show this message\n"
           " -noscale        Don't scale video output\n"
           " -scale          Double output pixels (default)\n"
           " -scanlines      Interleaved scanline mode\n"
           " -windowed       Run in a window (default)\n"
           " -fullscreen     Run fullscreen\n"
           " -width          Set window width\n"
           " -height         Set window height\n"
           " -forcesram      Force battery-backed ram\n"          
           " -despair        Disable joystick input\n"
           " -lockedout      Disable keyboard input\n"
           " -joy0 [A],[B],[SELECT],[START]\n"
           " -joy1 [A],[B],[SELECT],[START]\n"
           " -joy2 [A],[B],[SELECT],[START]\n"
           " -joy3 [A],[B],[SELECT],[START]\n"
           "                 Configure controller buttons for joypads 0...3\n"
           " -record FILE    Record controller input to file\n"
           " -play FILE      Play back controller input from file\n"
           " -playquit       Quit after movie playback is complete\n"
           " -restorestate   Restore from last saved state at startup\n"
           " -stripe FILE    Dump a vertical stripe of video to a file\n"
           " -stripex X      X coordinate of stripe (default is 128)\n"
           " -striperate FRAMES (default=1)\n"
           "                 Interval at which to sample stripe\n"
           " -nosound        Disable sound output\n"
           " -sound          Enable sound output (default)\n"
           " -nothrottle     Run at full speed, don't throttle to 60 Hz\n"
           " -trapbadops     Trap bad opcodes\n"
           " -debugbrk       Display BRK instructions\n"
           " -pputrace       Print PPU trace\n"
#ifdef DEBUG
           " -cputrace       Print CPU trace\n"
#endif
#ifdef USE_FUSE
           " -mountpoint PATH  Path at which to mount FUSE filesystem.\n"
           "                   Default is /tmp/nesfs\n"
           " -mountfs          Mount FUSE filesystem at startup.\n"
           " -nomountfs        Don't mount FUSE filesystem at startup.\n"
#endif
           "\n"
           "If no joystick is attached, keyboard controls will be used. The arrow keys\n"
           "control the D-pad, and the buttons are mapped as follows:\n"
           "   A      s\n"
           "   B      a\n"
           "   SELECT Tab\n"
           "   START  Enter\n"
           "\n"
           "The escape key will exit the emulator immediately. Alt-Enter toggles\n"
           "fullscreen mode. F5 and F7 respectively save and restore state. Backspace\n"
           "performs a hard reset. Control-s toggles sound. Control-a toggles continuous\n"
           "held autofire on button A, intended for unattended purchasing of items in\n"
           "certain role playing games. Control-d ends movie recording, when active.\n"
           "\n"
           "Diagnostic controls: F10 toggles instruction tracing. F11 toggles PPU tracing.\n"
           "Control-m toggles the mirroring mode. F12 enables additional miscellaneous\n"
           "output.\n"
        );
           
}

void cfg_parseargs (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++) {
    char *txt = argv[i];
    if (txt[0] == '-') {

      if (!strcmp(txt, "-help") || !strcmp(txt, "--help")) print_usage();
      if (!strcmp(txt, "-nosound")) sound_globalenabled = 0;
      if (!strcmp(txt, "-sound")) sound_globalenabled = 1;
      if (!strcmp(txt, "-nothrottle")) {
          no_throttle = 1;
          sound_globalenabled = 0;
      }
      if (!strcmp(txt, "-width")) {
	if (i != (argc - 1)) {
	  i++;
	  window_width = atoi(argv[i]);
	} else {
	  printf ("-width requires a numeric value.\n");
	}
      }
      if (!strcmp (txt, "-height")) {
	if (i != (argc - 1)) {
	  i++;
	  window_height = atoi(argv[i]);
	} else {
	  printf ("-height requires a numeric value.\n");
	}
      }
      if (!strcmp(txt, "-mapper")) {
	if (i != (argc - 1)) {
	  i++;
	  forcemapper = atoi(argv[i]);
	} else {
	  printf ("-mapper requires a numeric value.\n");
	}
      }
      if (!strcmp(txt, "-fullscreen")) vid_fullscreen = 1;
      if (!strcmp(txt, "-windowed")) vid_fullscreen = 0;
      if (!strcmp(txt, "-cputrace")) cputrace = 1;
      if (!strcmp(txt, "-pputrace")) trace_ppu_writes = 1;
      if (!strcmp(txt, "-debugbrk")) debug_brk = 1;
      if (!strcmp(txt, "-trapbadops")) cfg_trapbadops = 1;      
      if (!strcmp(txt, "-forcesram")) nes.rom.flags|=2;
      if (!strcmp(txt, "-diagnostic")) {
	cfg_diagnostic = 1;
	window_width = 640;
	window_height = 480;
      }

      if (!strcmp(txt, "-superverbose")) superverbose = 1;

      if (!strcmp(txt, "-despair")) cfg_disable_joysticks = 1;

      if (!strcmp(txt, "-lockedout")) cfg_disable_keyboard = 1;

      if (!strcmp(txt, "-jsmap")) {
	if (argc<=(++i)) break;
        // FIXME: Check these inputs.
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

      if (!strcmp(txt, "-record")) {
          if (argc<=(++i)) break;
          movie_output_filename = argv[i];
      }

      if (!strcmp(txt, "-playquit")) {
          quit_after_playback = 1;
      }

      if (!strcmp(txt, "-play")) {
          if (argc<=(++i)) break;
          movie_input_filename = argv[i];
      }

      if (!strcmp(txt, "-stripe")) {
          if (argc<=(++i)) break;
          video_stripe_output = fopen(argv[i], "wb");
          if (!video_stripe_output)
              printf("Unable to create \"%s\" for stripe output.\n", argv[i]);
      }

      if (!strcmp(txt, "-stripex")) {
          if (argc<=(++i)) break;
          video_stripe_idx = atoi(argv[i]);
          if (video_stripe_idx > 255) {
              video_stripe_idx = 128;
              printf("Stripe X value is out of range (0 to 255)\n");
          }
      }

      if (!strcmp(txt, "-striperate")) {
          if (argc<=(++i)) break;
          video_stripe_rate = atoi(argv[i]) - 1;          
      }      

      if (!strcmp(txt, "-noscale")) vid_filter = no_filter;
      if (!strcmp(txt, "-scale")) vid_filter = rescale_2x;
      if (!strcmp(txt, "-scanline")) vid_filter = scanline_filter;

      if (!strcmp(txt, "-restorestate")) startup_restore_state = 1;

#ifdef USE_FUSE
      if (!strcmp(txt, "-mountpoint")) {
          if (argc<=(++i)) break;
          cfg_mountpoint = argv[i];
      }

      if (!strcmp(txt, "-mountfs")) cfg_mount_fs = 1;
      if (!strcmp(txt, "-nomountfs")) cfg_mount_fs = 0;

#endif

    } else {
      romfilename = txt;
    }
  }

  if (!strlen(romfilename)) {
      print_usage();
      exit(1);
  }
}
