
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "sys.h"
#include "nes.h"
#include "sound.h"
#include "vid.h"
#include "config.h"


int keyboard_controller = 0;
int runflag = 1;

void dump_instruction_trace (void)
{
#ifdef INSTUCTION_TRACING
  FILE *trace, *mem;
  unsigned i,j;
  long long totals[3]={0,0,0};

  trace=fopen("trace-6502","wb");
  mem=fopen("dump-6502","wb");

  if (trace) {
    fwrite(tracing_counts,sizeof(tracing_counts),1,trace);
    fclose(trace);
  } else perror("Could not create instruction trace");

  if (mem) {
    for (i=0; i<0x10000; i++) fputc(Rd6502(i),mem);
    fclose(mem);
  }  

  for (i=0; i<0x10000; i++)
    for (j=0; j<3; j++) {
      totals[j]+=tracing_counts[i][j];
    }

  printf("Totals: %lli reads, %lli writes, and %lli executed.\n",totals[0],totals[1],totals[2]);
#endif
}

void process_key_event (SDL_KeyboardEvent * key)
{
    int idx;
    int symtable[8] = { SDLK_LMETA, SDLK_LCTRL, SDLK_TAB, SDLK_RETURN, SDLK_UP, SDLK_DOWN,
                        SDLK_LEFT, SDLK_RIGHT
    };
    for (idx = 0; idx < 8; idx++) {
        if (symtable[idx] == key->keysym.sym)
            break;
    }
    if (idx < 8) {
        switch (key->type) {
        case SDL_KEYUP:
            nes.joypad.pad[keyboard_controller][idx] = 0;
            break;
        case SDL_KEYDOWN:
            nes.joypad.pad[keyboard_controller][idx] = 1;
            break;
        default:
            break;
        }
    } else if (key->type==SDL_KEYUP) {
        switch (key->keysym.sym) {
        case SDLK_ESCAPE:
            runflag = 0;
            break;

#ifdef INSTRUCTION_TRACING
        case SDLK_F9:
            printf("Writing instruction trace... ");
            dump_instruction_trace();
            printf("done.\n");
            break;
#endif
            
        case SDLK_F12:            
            superverbose^=1;
            break;
            
        case SDLK_F11: 
            printf("Toggled PPU write trace.\n");
            trace_ppu_writes^=1;
            break;
    

        case SDLK_F10:
            printf("Toggled CPU trace.\n");
            nes.cpu.Trace = (~nes.cpu.Trace) & 1;
            break;

        case SDLK_BACKSPACE: 
            reset_nes(&nes); 
            break;

        case SDLK_m:
            if (key->keysym.mod & KMOD_CTRL) {
                nes.rom.mirror_mode ^= 1;
                printf("Toggled mirror mode. New mode=%i\n", nes.rom.mirror_mode);
            }
          
        default: break;
        }
    }
}

void process_joystick (int controller)
{
  int x,y,i;
  SDL_Joystick *joy = joystick[cfg_jsmap[controller]];
  SDL_JoystickUpdate ();
  x = SDL_JoystickGetAxis (joy, 0);
  y = SDL_JoystickGetAxis (joy, 1);  
  
  for (i=0; i<4; i++) {
    nes.joypad.pad[controller][i] = SDL_JoystickGetButton (joy, cfg_buttonmap[controller][i]);
  }

  for (i=4; i<8; i++) nes.joypad.pad[controller][i] = 0;

  if (y>cfg_joythreshold) nes.joypad.pad[controller][5] = 1;
  else if (y<(-cfg_joythreshold)) nes.joypad.pad[controller][4] = 1;

  if (x>cfg_joythreshold) nes.joypad.pad[controller][7] = 1;
  else if (x<(-cfg_joythreshold)) nes.joypad.pad[controller][6] = 1;    
}
  


int main (int argc, char **argv)
{
  int framecount = 0, frameskip = 1, i;

  cfg_parseargs (argc, argv);

  nes.rom = load_nes_rom (romfilename);
  if (nes.rom.prg == NULL) {
/*	printf("Couldn't load rom.\n"); */
    return 1;
  }

  sys_init();
  snd_init();

  init_nes(&nes);
  reset_nes(&nes);
  nes.cpu.Trace = cputrace;

  printf ("starting execution.\n");

/*   for(i=0;i<60*4;i++) */
  while (runflag) {
    SDL_Event event;

    for (i=0; i<numsticks; i++) {
      if (joystick[i]) process_joystick (i);
    }

    gettimeofday(&time_frame_start, 0);
    nes_runframe();   

/*	vid_draw_cache_page(0,0,0,0);   
	vid_draw_cache_page(1,0,160,0);
	vid_drawpalette(256,0);
	vid_render_frame(192,64); */
    if (!framecount) {
      vid_render_frame (0, 0);

      if (cfg_diagnostic) vid_drawpalette (0, 256);

      if (vid_filter) vid_filter(post_surface);

      if (post_surface != window_surface) {
	SDL_BlitSurface(post_surface,NULL,window_surface,NULL);
      }
     
      SDL_Flip (window_surface);
      sys_framesync();
    }
    while (SDL_PollEvent (&event)) {
      switch (event.type) {
      case SDL_QUIT:
	runflag = 0;
	break;

      case SDL_KEYDOWN:
	  process_key_event (&event.key);
	  break;

      case SDL_KEYUP:
	  process_key_event (&event.key);
	  break;

      default:
	break;
      }
    }

    frame_number++;
    framecount++;
    if (framecount == frameskip) framecount = 0;
  }

  nes.mapper->mapper_shutdown ();
  free_rom (&nes.rom);
  printf ("Rom freed.\n");

  printf ("Shutting down audio.\n");
  snd_shutdown ();
  printf ("Shutting down system.\n");
  sys_shutdown ();
  return 0;
}
