
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "sys.h"
#include "nes.h"
#include "sound.h"
#include "vid.h"
#include "config.h"

int keyboard_controller = 0;
int hold_button_a = 0;
int running = 1;

extern unsigned buffer_high;

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

void process_control_key (SDLKey sym)
{
    switch (sym) {
    case SDLK_m:
        nes.rom.mirror_mode ^= 1;
        printf("Toggled mirror mode. New mode=%i\n", nes.rom.mirror_mode);
        break;
        
    case SDLK_c:
        running = 0;
        break;
        
    case SDLK_s:
        sound_muted ^= 1;
        printf("Sound %s.\n", sound_muted? "muted" : "unmuted");
        break;
        
    case SDLK_a:
        hold_button_a ^= 1;
        nes.joypad.pad[0][0] = 0;
        printf("%s button A. Press Control-A to toggle.\n", hold_button_a? "holding" : "released");

    default: break;
    }
}

void process_key_event (SDL_KeyboardEvent * key)
{
    int idx;
    int symtable[8] = { SDLK_s, SDLK_a, SDLK_TAB, SDLK_RETURN, SDLK_UP, SDLK_DOWN,
                        SDLK_LEFT, SDLK_RIGHT };

    if ((key->keysym.mod & KMOD_CTRL) && (key->type == SDL_KEYUP)) {
        process_control_key(key->keysym.sym);
        return;
    }

    if ((key->keysym.mod & KMOD_ALT) && (key->type == SDL_KEYUP) && (key->keysym.sym == SDLK_RETURN)) {
        SDL_WM_ToggleFullScreen(window_surface);
        return;
    }

    for (idx = 0; idx < 8; idx++) {
        if (symtable[idx] == key->keysym.sym) break;
    }

    if (idx < 8) {
        switch (key->type) {
        case SDL_KEYUP:
            nes.joypad.pad[keyboard_controller][idx] = 0;
            break;
        case SDL_KEYDOWN:
            nes.joypad.pad[keyboard_controller][idx] = 1;
            break;
        default: break;
        }
    } else if (key->type==SDL_KEYUP) {

        switch (key->keysym.sym) {
        case SDLK_ESCAPE:
            running = 0;
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
            nes.cpu.Trace ^= 1;
            break;

        case SDLK_BACKSPACE: 
            reset_nes(&nes); 
            break;

        case SDLK_t:
            nes.cpu.Trace ^= 1;
            break;
            
        case SDLK_F5:
            save_state();
            break;
            
        case SDLK_F7:
            /* If the state restore fails, the state of the machine will be corrupt, so reset. */
            if (!restore_state()) reset_nes(&nes);
            break;

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
    nes.joypad.pad[controller][i] = SDL_JoystickGetButton(joy, cfg_buttonmap[controller][i]);
  }

  for (i=4; i<8; i++) nes.joypad.pad[controller][i] = 0;

  if (y>cfg_joythreshold) nes.joypad.pad[controller][5] = 1;
  else if (y<(-cfg_joythreshold)) nes.joypad.pad[controller][4] = 1;

  if (x>cfg_joythreshold) nes.joypad.pad[controller][7] = 1;
  else if (x<(-cfg_joythreshold)) nes.joypad.pad[controller][6] = 1;    
}
  
int main (int argc, char **argv)
{
    int i;
    unsigned frame_start_cycles = 0;
    cfg_parseargs (argc, argv);

    nes.rom = load_nes_rom (romfilename);
    if (nes.rom.prg == NULL) {
        printf("Unable to load rom.\n");
        return 1;
    }

    sys_init();
    snd_init();
    init_nes(&nes);
    reset_nes(&nes);
    nes.cpu.Trace = cputrace;

    time_frame_target = usectime();

    while (running) {
        SDL_Event event;

        for (i=0; i<numsticks; i++) if (joystick[i]) process_joystick(i);
        if (hold_button_a) nes.joypad.pad[0][0] = frame_number & 1;

        time_frame_start = usectime();
        while (time_frame_target <= time_frame_start) time_frame_target += (1000000ll / 60ll);
        frame_start_samples = buffer_high;

        render_clear();
        nes_runframe();

        //vid_render_frame(0, 0);
        //if (nes.ppu.control2 & 0x10) vid_draw_sprites(0,0);

        SDL_Flip(window_surface);
        sys_framesync();
 
        if (0)
        printf("Frame cycles: %i (expect %i samples, actually generated %i samples)\n",
               (nes.cpu.Cycles - frame_start_cycles)/120,
               (nes.cpu.Cycles - frame_start_cycles)/4467, 
               buffer_high - frame_start_samples);

        frame_start_cycles = nes.cpu.Cycles;

        while (SDL_PollEvent (&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;
                
            case SDL_KEYDOWN:
                process_key_event(&event.key);
                break;
                
            case SDL_KEYUP:
                process_key_event(&event.key);
                break;
                
            default:
                break;
            }
        }

        frame_number++;
        /* Automatically save SRAM to disk once per minute. */
        if ((frame_number > 0) && !(frame_number % 3600)) save_sram(nes.save, &nes.rom, 0);
  }

  save_sram(nes.save, &nes.rom, 1);
  mapper->mapper_shutdown ();
  free_rom (&nes.rom);
  printf ("Rom freed.\n");

  printf ("Shutting down audio.\n");
  snd_shutdown();
  printf ("Shutting down system.\n");
  sys_shutdown();
  return 0;
}
