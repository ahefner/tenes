
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sys.h"
#include "nes.h"
#include "sound.h"
#include "vid.h"
#include "config.h"
#include "nespal.h"
#include "font.h"
#include "filesystem.h"
#include "ui.h"


int hold_button_a = 0;

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

void palette_dump (void)
{
    printf("-------------- Palette Dump -------------\n");
    for (int n=0; n<0x20; n++) {
        int pal = nes.ppu.vram[0x3F00+n];
        printf("Color %02X = %02X (%3i,%3i,%3i)\n", n, pal, 
               nes_palette[pal*3+0], nes_palette[pal*3+1], nes_palette[pal*3+2]);
    }    
    for (int addr=0x3F00; addr<0x3F20; addr++) printf("%02X ", nes.ppu.vram[addr]);
    printf("\n-----------------------------------------\n");
}

/* Assume axes 2 and 3 are analog and need to be calibrated. The
 * center will be sampled at the start of the program, and can be
 * recalibratead to the current center at any time by pressing
 * Control-j. */
void calibrate_aux_stick (void)
{
    struct joystick *joy = &joystick[cfg_jsmap[0]];
    if (joy->connected) {
        aux_axis[0] = SDL_JoystickGetAxis(joy->sdl, 2);
        aux_axis[1] = SDL_JoystickGetAxis(joy->sdl, 3);
    }
}

int screencapping = 0;
char screencap_dest[256];

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

    case SDLK_d:
        if (movie_output) {
            printf("Stopped recording movie.\n");
            fclose(movie_output);
            movie_output = NULL;
        } else printf("Not currently recording a movie.\n");
        break;
        
    case SDLK_s:
        sound_muted ^= 1;
        printf("Sound %s.\n", sound_muted? "muted" : "unmuted");
        break;

    case SDLK_j:
        calibrate_aux_stick();
        break;
        
    case SDLK_a:
        hold_button_a ^= 1;
        nes.joypad.pad[0] &= ~1;
        printf("%s button A. Press Control-A to toggle.\n", hold_button_a? "holding" : "released");
        break;

    case SDLK_p:
        palette_dump();
        break;

    case SDLK_F12:
        if (screencapping) {
            screencapping = 0;
            printf("Ended screencapping. Files saved to %s\n", screencap_dest);
        } else {
            screencapping = 1;
            snprintf(screencap_dest, sizeof(screencap_dest), "./screencaps-%i/", (int)time(NULL));
            mkdir(screencap_dest, 0777);
        }

    default: break;
    }
}



/* Not Menu */

byte keyboard_input = 0;

void process_key_event (SDL_KeyboardEvent * key)
{
    int idx;

    // Control and alt keys are available globally.
    // Ignore modified keys down, so as not to confuse the input code.
    if (((key->keysym.mod & KMOD_CTRL) || (key->keysym.mod & KMOD_ALT)) 
        && (key->type == SDL_KEYDOWN)) return;

    // Actions occur on key up.
    if ((key->keysym.mod & KMOD_CTRL ) && (key->type == SDL_KEYUP)) {
        process_control_key(key->keysym.sym);
        return;
    }
    
    if ((key->keysym.mod & KMOD_ALT) && 
        (key->type == SDL_KEYUP) && 
        (key->keysym.sym == SDLK_RETURN)) 
    {
        SDL_WM_ToggleFullScreen(window_surface);
        return;
    }
    
    // If the menu is open, pass keystrokes through to it.
    if (menu && menu_process_key_event(key)) return;

    // Match keysym against keyboard -> button mapping:
    if (cfg_disable_keyboard) idx = 8;
    else {
        for (idx = 0; idx < 8; idx++) {
            if (keymap[idx] == key->keysym.sym) break;
        }
    }

    if (idx < 8) {
        switch (key->type) {
        case SDL_KEYUP:
            keyboard_input &= ~BIT(idx);
            break;
        case SDL_KEYDOWN:
            keyboard_input |= BIT(idx);
            break;
        default: break;
        }
    } else if (key->type==SDL_KEYUP) {

        switch (key->keysym.sym) {
        case SDLK_ESCAPE:
            //running = 0;
            open_menu();
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
            save_state_to_disk(NULL);
            break;
            
        case SDLK_F7:
            if (!restore_state_from_disk(NULL)) reset_nes(&nes);
            break;

        default: break;
        }
    }
}

void process_joystick (int controller)
{
    int x,y;
    struct joystick *joy = &joystick[cfg_jsmap[controller]];
    SDL_JoystickUpdate();
    x = SDL_JoystickGetAxis(joy->sdl, 0);
    y = SDL_JoystickGetAxis(joy->sdl, 1);
    aux_position[0] = max(-1.0, min(1.0, (SDL_JoystickGetAxis(joy->sdl, 2) - aux_axis[0])/25000.0));
    aux_position[1] = max(-1.0, min(1.0, (SDL_JoystickGetAxis(joy->sdl, 3) - aux_axis[1])/25000.0));
    
    nes.joypad.pad[controller] = 0;
    
    for (int i=0; i < 4; i++) {
        if (SDL_JoystickGetButton(joy->sdl, cfg_buttonmap[controller][i]))
            nes.joypad.pad[controller] |= BIT(i);
    }

    if (y > cfg_joythreshold) nes.joypad.pad[controller] |= BIT(5);
    else if (y < (-cfg_joythreshold)) nes.joypad.pad[controller] |= BIT(4);

    if (x > cfg_joythreshold) nes.joypad.pad[controller] |= BIT(7);
    else if (x < (-cfg_joythreshold)) nes.joypad.pad[controller] |= BIT(6);
}

void process_events (struct inputctx *input)
{
    SDL_Event event;

    input->pressed = 0;
    input->released = 0;
    input->buttons = SDL_GetMouseState(&input->mx, &input->my);

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

        case SDL_MOUSEBUTTONDOWN:
            input->pressed |= SDL_BUTTON(event.button.button);
            break;
            
        case SDL_MOUSEBUTTONUP:
            input->released |= SDL_BUTTON(event.button.button);
            break;
            
        default:
            break;
        }
    }
}

unsigned frame_start_cycles = 0;

void runframe (void)
{
    unique_frame_number++;

    if (movie_input) {
        byte tmp[4];
        if (fread(&tmp, 4, 1, movie_input)) {
            for (byte i=0; i<4; i++) nes.joypad.pad[i] ^= tmp[i];
        } else {
            printf("Reached end of movie '%s'\n", movie_input_filename);
            fclose(movie_input);
            movie_input = NULL;
            if (quit_after_playback) running=0;
        }
    }
    
    if (movie_output) {
        if (!fwrite(&nes.joypad.pad, 4, 1, movie_output)) {
            fclose(movie_output);
            movie_output = NULL;
        }
    }
    
    time_frame_start = usectime();
    while (time_frame_target <= time_frame_start) time_frame_target += (1000000ll / 60ll);
    frame_start_samples = buffer_high;
    frame_start_cycles = nes.cpu.Cycles;

    render_clear();
    nes_emulate_frame();

    if (screencapping) {
        char dest[256];
        snprintf(dest, sizeof(dest), "%s/%06i.bmp", screencap_dest, screencapping++);
        SDL_SaveBMP(window_surface, dest);
    }    
    
    /* Automatically save SRAM to disk once per minute. */
    if ((unique_frame_number > 0) && 
        !(unique_frame_number % 3600)) 
    {
        save_sram(nes.save, &nes.rom, 0);
    }
}

void describe_keymap (void)
{
    char *buttons[8] = { "A", "B", "Select", "Start", "Up", "Down", "Left", "Right"};
    for (int idx = 0; idx < 8; idx++) {
        printf("  %6s <= %s\n", buttons[idx], SDL_GetKeyName(keymap[idx]));
    }
}

void close_current_game (void)
{
    save_sram(nes.save, &nes.rom, 1);
    save_state_to_disk(state_filename(&nes.rom, 0));
    mapper->mapper_shutdown();
    free_rom(&nes.rom);
}

int open_game (char *filename)
{
    static int first_time = 1;
    nes.rom = load_nes_rom (filename);
    if (nes.rom.prg == NULL) {
        printf("Unable to load rom \"%s\".\n", filename);
        return 1;
    }
    
    save_pref_string("lastfile", filename);

    init_nes(&nes);
    reset_nes(&nes);

    if (startup_restore_state >= 0)
        restore_state_from_disk(state_filename(&nes.rom, startup_restore_state));
    
    /* First time through here, the video hasn't been initalized yet
     * (in case we can't open the rom file). */
    if (first_time) first_time = 0;
    else SDL_WM_SetCaption (nes.rom.title, nes.rom.title);

    return 0;
}


int main (int argc, char **argv)
{
    memset(joystick, 0, sizeof(joystick));
    load_config();
    cfg_parseargs(argc, argv);

    if (ensure_freetype() != 1) {
        /* TODO: Do some magic to disable the UI here. */
    }

    if (open_game(romfilename)) return 1;
    
    if (movie_input_filename) {
        movie_input = fopen(movie_input_filename, "rb");
        if (!movie_input) printf("Unable to open '%s' for movie input.\n", movie_input_filename);
    }

    if (movie_output_filename) {
        movie_output = fopen(movie_output_filename, "wb");
        if (!movie_output) printf("Unable to create '%s' for movie output.\n", movie_output_filename);
        else printf("Recording movie to '%s'\n", movie_output_filename);
    }

    sys_init();   

    fs_add_chunk("ram", nes.ram, sizeof(nes.ram), 1);
    fs_add_chunk("sram", nes.save, sizeof(nes.save), 1);
    fs_add_chunk("vram", nes.ppu.vram, sizeof(nes.ppu.vram), 1);
    fs_add_chunk("oam", nes.ppu.spriteram, sizeof(nes.ppu.spriteram), 1);
    fs_add_chunk("chr-rom", nes.rom.chr, nes.rom.chr_size, 1);
    fs_add_chunk("prg-rom", nes.rom.prg, nes.rom.prg_size, 1);
    fs_add_chunk("rom-filename", nes.rom.filename, strlen(nes.rom.filename), 0);

    static char hashbuf[256];
    sprintf(hashbuf, "%llX\n", (unsigned long long)nes.rom.hash);
    fs_add_chunk("rom-hash", hashbuf, strlen(hashbuf), 0);

    if (joystick[0].connected) calibrate_aux_stick();
    if (snd_init() == -1) sound_globalenabled = 0;

    //if (!cfg_disable_keyboard) describe_keymap();

#ifdef USE_FUSE
    if (cfg_mount_fs) fs_mount(cfg_mountpoint);
#endif

    time_frame_target = usectime();

    while (running) {
        memset(window_surface->pixels, 0, window_surface->pitch * window_surface->h);

        struct inputctx ctx;
        process_events(&ctx);

        for (int i=0; i<4; i++) nes.joypad.pad[i] = 0;
        for (int i=0; i<numsticks; i++) if ((cfg_jsmap[i]>=0) && joystick[cfg_jsmap[i]].connected) process_joystick(i);
        if (hold_button_a) nes.joypad.pad[0] |= nes.time & 1;

        nes.joypad.pad[cfg_keyboard_controller] |= keyboard_input;

        runframe();
        dim_background();

        if (menu) run_menu(&ctx);
        else dim_y_target = 0;

        SDL_Flip(window_surface);
        if (!no_throttle) sys_framesync();
    
        if (0)
            printf("Frame cycles: %i (expect %i samples, actually generated %i samples)\n",
                   (nes.cpu.Cycles - frame_start_cycles)/120,
                   (nes.cpu.Cycles - frame_start_cycles)/4467, 
                   buffer_high - frame_start_samples);
    }

    close_current_game();

    if (movie_output) fclose(movie_output);
    if (movie_input) fclose(movie_input);
    if (video_stripe_output) fclose(video_stripe_output);

    save_config();

    printf("Shutting down audio.\n");
    snd_shutdown();
    printf("Shutting down system.\n");
    sys_shutdown();
    return 0;
}
