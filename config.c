#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#ifndef _WIN32
#include <pwd.h>
#endif

#include "global.h"
#include "sys.h"

void scan_video_option (const char *arg)
{
    void *mode = NULL;

    if (arg == NULL) return;
    if (!strcmp(arg, "noscale")) mode = no_filter;
    if (!strcmp(arg, "scale")) mode = rescale_2x;
    if (!strcmp(arg, "scanline")) mode = scanline_filter;
    if (!strcmp(arg, "ntsc")) mode = ntsc_filter;
    if (!strcmp(arg, "ntsc2x")) mode = ntsc2x_filter;

    if (mode) {
        save_pref_string("video-mode", arg);
        vid_filter = mode;
    }
}

void print_usage (void)
{
    printf("Usage: nesemu [OPTION]... ROMFILE\n"
           "Emulate an NTSC NES running ROMFILE (in iNES .nes format)\n"
           "\n"
           "Options:\n"
           " -help           Show this message\n"
           " -noscale        Don't scale video output\n"
           " -scale          Double output pixels\n"
//           " -scanline       Interleaved scanline mode\n"
           " -ntsc           Use NTSC filter (default)\n"
           " -ntsc2x         Doubled NTSC filter\n"
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
           " -restorestate   Restore from last saved state at startup (default)\n"
           " -reset          Reset NES at startup\n"
           " -stripe FILE    Dump a vertical stripe of video to a file\n"
           " -stripex X      X coordinate of stripe (default is 128)\n"
           " -striperate FRAMES (default=1)\n"
           "                 Interval at which to sample stripe\n"
           " -nosound        Disable sound output\n"
           " -sound          Enable sound output (default)\n"
           " -nothrottle     Run at full speed, don't throttle to 60 Hz\n"
           " -trapbadops     Trap bad opcodes\n"
           " -debugbrk       Display BRK instructions\n"
           " -tracewr        Trace all memory writes\n"
           " -apudump FILE   Log APU writes to file (for audio ripping)\n"
           " -pputrace       Print PPU trace\n"
#ifdef DEBUG
           " -cputrace       Print CPU trace\n"
#endif
#ifdef USE_FUSE
           "\n"
           " -mountpoint PATH  Path at which to mount FUSE filesystem.\n"
           "                   Default is /tmp/nesfs\n"
           " -mountfs          Mount FUSE filesystem at startup.\n"
           " -nomountfs        Don't mount FUSE filesystem at startup.\n"
           "\n"
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

void cfg_parseargs (int argc, const char **argv)
{
  int i;
  for (i = 1; i < argc; i++) {
    const char *txt = argv[i];

    if (txt[0] == '-') {

      scan_video_option(txt+1);

      if (!strcmp(txt, "-help") || !strcmp(txt, "--help")) {
          print_usage();
          return;
      }

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
      if (!strcmp(txt, "-tracewr")) trace_mem_writes = 1;
      if (!strcmp(txt, "-pputrace")) trace_ppu_writes = 1;
      if (!strcmp(txt, "-debugbrk")) debug_brk = 1;
      if (!strcmp(txt, "-trapbadops")) cfg_trapbadops = 1;
      if (!strcmp(txt, "-forcesram")) nes.rom.flags|=2;
      if (!strcmp(txt, "-diagnostic")) {
	cfg_diagnostic = 1;
	window_width = 640;
	window_height = 480;
      }

      if (!strcmp(txt, "-apudump") && (i != argc-1)) {
          i++;
          const char *outfile = argv[i++];
          printf("Dumping APU register dump to \"%s\"\n", outfile);
          apu_dump = calloc(1, sizeof(struct apulog));
          apu_dump->out = fopen(outfile, "wb");
          if (!apu_dump->out) {
              printf("Couldn't open \"%s\" for writing: %s\n", outfile, strerror(errno));
              exit(1);
          }
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
          int tmp;
          if (argc<=(++i)) break;
          tmp = atoi(argv[i]);
          video_stripe_idx = tmp;
          if (tmp > 255) {
              video_stripe_idx = 128;
              printf("Stripe X value is out of range (0 to 255)\n");
          }
      }

      if (!strcmp(txt, "-striperate")) {
          if (argc<=(++i)) break;
          video_stripe_rate = atoi(argv[i]) - 1;
      }



      if (!strcmp(txt, "-restorestate")) startup_restore_state = 1;
      if (!strcmp(txt, "-reset")) startup_restore_state = -1;

#ifdef USE_FUSE
      if (!strcmp(txt, "-mountpoint")) {
          if (argc<=(++i)) break;
          cfg_mountpoint = argv[i];
      }

      if (!strcmp(txt, "-mountfs")) cfg_mount_fs = 1;
      if (!strcmp(txt, "-nomountfs")) cfg_mount_fs = 0;

#endif

    } else {
        if (romfilename) free(romfilename);
        romfilename = make_absolute_filename(txt);
    }
  }

  /* If no name supplied, try the last file loaded.. but, if it
     doesn't exist, I'd rather pretend we didn't and show the
     usage message as normal, rather than confuse the user by
     erring on a file they didn't explicitly specify. */
  if (!romfilename) {
      romfilename = pref_string("lastfile", NULL);
      if (!romfilename || !probe_file(romfilename)) romfilename = NULL;
  }

  if (!romfilename) {
      print_usage();
      exit(1);
  }
}

/* User preferences and save data */

char config_dir[PATH_MAX];
const char *ensure_config_dir (void)
{
#ifdef _WIN32
    snprintf(config_dir, sizeof(config_dir), "C:\\fixme\\");
#else
    struct passwd *pwd = getpwuid(getuid());

    if (getenv("HOME")) {
        snprintf(config_dir, sizeof(config_dir), "%s/.tenes", getenv("HOME"));
    } else if (!pwd) {
        snprintf(config_dir, sizeof(config_dir), "/tmp/tenes-%i", (int)getuid());
    } else {
        snprintf(config_dir, sizeof(config_dir), "%s/.tenes", pwd->pw_dir);
    }

#endif

    make_dir(config_dir);
    return config_dir;
}

char save_dir[PATH_MAX];
const char *ensure_save_dir (void)
{
    snprintf(save_dir, sizeof(save_dir), "%s/sram", ensure_config_dir());
    make_dir(save_dir);
    return save_dir;
}

char state_dir[PATH_MAX];
const char *ensure_state_dir (long long hash)
{
    snprintf(state_dir, sizeof(state_dir), "%s/state", ensure_config_dir());
    make_dir(state_dir);
    unsigned lower = hash & 0xFFFFFFFF;
    unsigned upper = hash >> 32;
    snprintf(state_dir, sizeof(state_dir), "%s/state/%X%X", ensure_config_dir(), upper, lower);
    make_dir(state_dir);
    return state_dir;
}

const char *sram_filename (struct nes_rom *rom)
{
    static char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%X%X", ensure_save_dir(),
             (unsigned)(rom->hash >> 32),
             (unsigned)(rom->hash & 0xFFFFFFFFll));
    return path;
}

const char *state_filename (struct nes_rom *rom, unsigned index)
{
    static char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%02X", ensure_state_dir(rom->hash), index);
    return path;
}

const char *pref_filename (const char *name)
{
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/%s", ensure_config_dir(), name);
    return buf;
}

/* The caller is responsible for freeing the resulting string. If the
 * defaultval is used, a copy is returned. */
char *pref_string (const char *name, const char *defaultval)
{
    /* Load binary file adds a terminator for us, just in case. */
    char *string = (char *)load_binary_file(pref_filename(name), NULL);
    if ((string==NULL) && (defaultval!=NULL)) return strdup(defaultval);
    else return string;
}

void save_pref_file (const char *name, const byte *data, size_t size)
{
    save_binary_data(pref_filename(name), data, size);
}

void save_pref_string (const char *name, const char *string)
{
    save_pref_file(name, (const byte *)string, strlen(string));
}

void save_pref_int (const char *name, int n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%i", n);
    save_pref_string(name, buf);
}

int pref_int (const char *name, int defaultval)
{
    char *result;
    result = pref_string(name,NULL);
    if (result != NULL) {
        int value = atoi(result);
        free(result);
        return value;
    } else return defaultval;
}

void load_config (void)
{
    char *video = pref_string("video-mode", NULL);
    char *muted = pref_string("sound-muted", "");

    scan_video_option(video);
    if (video) free(video);

    if (!strcmp(muted, "true")) sound_muted = 1;
    free(muted);

    cfg_keyboard_controller = pref_int("keyboard-controller", 0);
}

void save_config (void)
{
    save_pref_string("sound-muted", sound_muted? "true" : "false");
    save_pref_int("keyboard-controller", cfg_keyboard_controller);
}
