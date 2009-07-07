
#define NES_C

#include <assert.h>
#include <sys/stat.h>
#include <limits.h>
#include "sys.h"
#include "nes.h"
#include "sound.h"
#include "dasm.c"
#include "vid.h"
#include "config.h"
#include "global.h"

/* init_nes - initializes a nes_machine struct that already had it's rom member set properly */
/*            after this, an nes_reset is all it should take to start the first frame.       */
void init_nes (struct nes_machine *nes)
{
#ifdef INSTUCTION_TRACING
  memset(tracing_counts,0,sizeof(tracing_counts));
#endif

  if (forcemapper!=-1) {
    printf ("Forcing mapper to %i\n", forcemapper);
    nes->rom.mapper = forcemapper;
    nes->rom.mapper_info = mapper_find(forcemapper);
  }

  if (nes->rom.mapper_info) {
    printf ("Mapper is \"%s\"\n", nes->rom.mapper_info->name);
    if (nes->rom.mapper_info->methods) mapper = nes->rom.mapper_info->methods;
    else {
        printf("Warning: This mapper is not currently implemented.\n");
        mapper = &mapper_None;        
    }
  } else {
      printf ("Unknown mapper (%i). Defaulting to mapper 0.\n", nes->rom.mapper);
      mapper = &mapper_None;
  }

  nes->joypad.connected = 1;
  mapper->mapper_init ();
  nes->last_sound_cycle = 0;

  /* Load SRAM */
  memset((void *)nes->save, 0, 0x2000);
  if (nes->rom.flags & 2) {
      FILE *in = fopen(sram_filename(&nes->rom), "rb");
      if (in) {
          if (!fread(nes->save, 0x2000, 1, in))
              printf("Warning: Unable to read save file.\n");
          else printf("Loaded save data from %s\n", sram_filename(&nes->rom));
          fclose(in);
      }
  }


  printf ("NES initialized.\n");
}

void shutdown_nes (struct nes_machine *nes)
{
    mapper->mapper_shutdown();  
}

/* reset_nes - resets the state of the cpu, ppu, sound, joypads, and internal state */
void reset_nes (struct nes_machine *nes)
{
  memset ((void *) nes->ram, 0, 0x800);

  SDL_mutexP(producer_mutex);

  memset ((void *) &nes->snd, 0, sizeof (nes->snd));
  Reset6502(&nes->cpu);
  nes->last_sound_cycle = 0;
  assert(nes->cpu.Cycles == 0);
  assert(nes->last_sound_cycle == 0);
  
  SDL_mutexV(producer_mutex);

  if (cfg_trapbadops) nes->cpu.TrapBadOps = 1;
  else nes->cpu.TrapBadOps = 0;

  nes->scanline_start_cycle = 0;

  memset((void *)(nes->ppu.vram + 0x2000), 0, 0x2000);
  nes->ppu.control1 = 0;
  nes->ppu.control2 = 0;
  nes->ppu.v = 0;
  nes->ppu.t = 0;
  nes->ppu.x = 0;
  nes->ppu.sprite_address = 0;
  nes->ppu.read_latch = 0;
  nes->ppu.ppu_addr_mode = DUAL_HIGH;
  nes->ppu.ppu_writemode = PPUWRITE_HORIZ;
  nes->ppu.vblank_flag = 0;

  memset ((void *) nes->joypad.pad, 0, 4 * 8 * sizeof (int));
  nes->joypad.state[0] = 0;
  nes->joypad.state[1] = 0;

  rendering_scanline = 0;
  printf ("NES reset.\n");
}

char *sram_filename (struct nes_rom *rom)
{
    static char path[PATH_MAX];    
    snprintf(path, sizeof(path), "%s/%llX", ensure_save_dir(), rom->hash);
    return path;
}

char *state_filename (struct nes_rom *rom, unsigned index)
{
    static char path[PATH_MAX];    
    snprintf(path, sizeof(path), "%s/%02X", ensure_state_dir(rom->hash), index);
    return path;
}


int write_state_chunk (FILE *stream, char *name, void *data, Uint32 length)
{
    char chunkname[64];
    memset(chunkname, 0, sizeof(chunkname));
    strncpy(chunkname, name, sizeof(chunkname));
    if (fwrite(chunkname, sizeof(chunkname), 1, stream) != 1) return 0;
    if (fwrite(&length, sizeof(length), 1, stream) != 1) return 0;
    if (fwrite(data, length, 1, stream) != 1) return 0;
    return 1;
}

int read_state_chunk (FILE *stream, char *name, void *data_out, Uint32 length)
{
    char chunkname[64];
    Uint32 read_length;
    if (fread(chunkname, sizeof(chunkname), 1, stream) != 1) return 0;
    if (strncmp(chunkname, name, sizeof(chunkname))) { 
        printf("Wrong state chunk: Read \"%s\", expected \"%s\"\n", chunkname, name);
        return 0;
    }
    if (fread(&read_length, sizeof(read_length), 1, stream) != 1) return 0;
    if (read_length != length) {
        printf("Size of chunk \"%s\" is wrong. Expected %i, actually %i.", name, length, read_length); 
        return 0;
    }
    if (fread(data_out, read_length, 1, stream) != 1) return 0;
    return 1;
}

void save_state (void)
{
    char *filename = state_filename(&nes.rom, 0);
    FILE *out = fopen(filename, "wb");
    if (!out) printf("Unable to create state file %s\n", filename);
    else {
        if (!(write_state_chunk(out, "NES Machine", &nes, sizeof(nes)) &&
              mapper->save_state(out))) {
            printf("Error writing state file to %s\n", filename);
        }
        fclose(out);
    }
}

int restore_state (void)
{
    struct nes_rom rom;
    char *filename = state_filename(&nes.rom, 0);
    FILE *in = fopen(filename, "rb");

    /* Awesome kludge: preserve nes.rom structure, except for mirroring state. */
    memcpy(&rom, &nes.rom, sizeof(rom));

    if (!in) printf("Unable to open state file %s\n", filename);    
    else {
        if (!(read_state_chunk(in, "NES Machine", &nes, sizeof(nes)) &&
              mapper->restore_state(in))) {
            printf("Error restoring state file from %s\n", filename);
            fclose(in);
            int mirror_mode = nes.rom.mirror_mode;
            int onescreen_page = nes.rom.onescreen_page;
            memcpy(&nes.rom, &rom, sizeof(rom));
            nes.rom.mirror_mode = mirror_mode;
            nes.rom.onescreen_page = onescreen_page;
            return 0;
        }
        fclose(in);
    }

    memcpy(&nes.rom, &rom, sizeof(rom));
    return 1;
}

int ppu_current_hscroll (void)
{
    return nes.ppu.x + ((nes.ppu.v & 0x1F) << 3) + ((nes.ppu.v & 0x400) ? 256 : 0);
}

int ppu_current_vscroll (void)
{
    return ((((nes.ppu.v >> 5)&0x1F) << 3) + (nes.ppu.v >> 12) + ((nes.ppu.v & 0x800) ? 240 : 0)) % 480;
}

/* nes_initframe - call at the start of every frame */
void nes_initframe (void)
{
  nes.ppu.hit_flag = 0;	/* set to 0 at frame start */
  nes.ppu.vblank_flag=0; /* set to 0 at frame start, I think */
  nes.ppu.spritecount_flag = 0;
  nes.ppu.sprite_address = 0;
  nes.sprite0_detected = 0;

  if (nes.ppu.control2 & 0x18) nes.ppu.v = nes.ppu.t;

  nes.scanline = 0;
  emphasis_position = -1;
}

/* nes_vblankstate - sets things that change when a vblank occurs */
void nes_vblankstate (void)
{
  nes.ppu.vblank_flag = 1;
  nes.ppu.hit_flag = 0;
}

void joypad_write (word Addr)
{
  /* I'm taking a shortcut on the strobe here... */
  nes.joypad.state[(Addr == 0x4016) ? 0 : 1] = 0;
}

byte joypad_read (word Addr)
{
  int padindex = (Addr == 0x4016) ? 0 : 1;
  int state = nes.joypad.state[padindex];
  byte retval = 0;
  if (state < 8) {
    retval = GETBIT(state, nes.joypad.pad[padindex]);
  } else if (state < 16) {
    retval = GETBIT(state-8, nes.joypad.pad[padindex + 2]);
  } else {
    switch (state) {
    case 17:
    case 18:
      retval = 0;
      break;
    case 19:
      if (padindex)
	retval = 1;
      break;
    case 20:
      if (!padindex)
	retval = 1;
      break;
    default:
      break;
    }
  }
  nes.joypad.state[padindex]++;
  return retval;
}

int scanline_cycles (void)
{
    return (int)(nes.cpu.Cycles - nes.scanline_start_cycle) / MASTER_CLOCK_DIVIDER;
}

char *nes_time_string (void)
{
    static char buf[128];
    sprintf(buf, "%i/%3i.%3i   ", frame_number, nes.scanline, (int)scanline_cycles());
    return buf;
}

void nes_printtime (void)
{
    fputs(nes_time_string(), stdout);
}

void mapper_twiddle (void)
{
    if (mapper->scanline_end()) Int6502(&nes.cpu, INT_IRQ);
}

/*  CPU/Hardware interface.  */

static inline int in_vblank (struct nes_machine *nes)
{
    return nes->scanline >= 242;
}

void Wr6502 (register word Addr, register byte Value)
{
#ifdef INSTRUCTION_TRACING
  tracing_counts[Addr][COUNT_WRITE]++;
#endif

  switch (Addr & 0xE000) {
  case 0x0000:			/* ram - mirror in read function */
    nes.ram[Addr & 0x7FF] = Value;
    break;

  case 0x2000:			/* register */
      if (superverbose) {
          nes_printtime();
          printf("$%04X <- %02X\n", Addr, Value);
      }
      switch ((Addr & 0x0007) | 0x2000) {

      case ppu_cr1: /* 2000 */

	  nes.ppu.control1 = Value;
	  nes.ppu.ppu_writemode = (Value & 0x04) ? PPUWRITE_VERT : PPUWRITE_HORIZ;
          nes.ppu.t = (nes.ppu.t & 0xF3FF) | ((Value & 3) << 10);
          
          if (trace_ppu_writes)
              printf("%sCR1 write: ppu.t = %04X (selected nametable %i)\n", 
                     nes_time_string(), nes.ppu.t, Value&3);

	  if (trace_ppu_writes)
          {
	      nes_printtime ();
              printf("PPU $2000 = ");
	      PrintBin (Value);
	      printf(" next PC=$%04X, ",nes.cpu.PC.W);	      
	      if (Value & 0x80) printf ("NMI ON  ");
	      if (Value & 0x10) printf ("bg1 "); else printf ("bg0 ");
	      if (Value & 0x08) printf ("spr1 "); else printf ("spr0 ");
	      /*                 if(Value & 0x20) printf("8x16 sprites "); else printf("8x8 sprites "); */
	      if (Value & 0x04) printf ("Vertical write\n"); else printf ("Horiz write.\n");
	      /*                 if(!in_vblank(&nes)) printf("PPU: CR1 written during frame\n"); */
	      printf("\n");
          }
	  break;

	
      case ppu_cr2: /* 2001 */
          catchup_emphasis();
	  nes.ppu.control2 = Value;
	  if (trace_ppu_writes) {
            nes_printtime ();
             printf("PPU 2001: ");
	    if (Value & 0x10) printf ("sprites ON  ");
	    else printf ("sprites OFF  ");
	    if (Value & 0x08) printf ("bg ON  ");
	    else printf ("bg OFF  ");
            if (Value & 0x01) printf("MONO  "); 
            if (Value & 0xE0) printf("Emphasis %i", Value >> 5);
	    printf ("\n");
	    /*                 if(!in_vblank(&nes)) printf("PPU: CR2 written during frame\n"); */
	  }
	  break;



      case ppu_status: /* 2002 */
	  printf("PPU: Write to status register ???\n");
	  break;


      case spr_addr: /* 2003 */
	  nes.ppu.sprite_address = Value;
	  break;


      case spr_data: /* 2004 */
          //printf("Sprite[%i] = %i\n", (unsigned)nes.ppu.sprite_address, (unsigned)Value);
	  nes.ppu.spriteram[nes.ppu.sprite_address++] = Value;
          nes.ppu.sprite_address &= 0xFF;
	  break;


      case ppu_bgscroll: /* 2005 */
          if (nes.ppu.ppu_addr_mode == DUAL_HIGH) {
              nes.ppu.x = Value & 7;
              nes.ppu.t &= ~0x1F;
              nes.ppu.t |= Value >> 3;
          } else {
              nes.ppu.t &= ~0x73E0;
              nes.ppu.t |= (Value & 7) << 12;
              nes.ppu.t |= (Value >> 3) << 5;
          }
          nes.ppu.ppu_addr_mode ^= 1;
          if (trace_ppu_writes) {
              nes_printtime();
              printf("ppu.t = %04X (wrote %02X to %s scroll)\n", 
                     nes.ppu.t, Value, 
                     nes.ppu.ppu_addr_mode ? "horizontal" : "vertical");
          }
	  break;

      case ppu_addr: /* 2006 */

	  if (nes.ppu.ppu_addr_mode == DUAL_HIGH) {
              nes.ppu.t = (nes.ppu.t & 0x00FF) | ((Value&0x3F) << 8);            
          } else {
              nes.ppu.t = (nes.ppu.t & 0xFF00) | Value;
              nes.ppu.v = nes.ppu.t;
              if (trace_ppu_writes) {
                  nes_printtime();
                  printf("ppu.v = %04X ($2006, latched from ppu.t)\n", nes.ppu.v);
              }
	  }
          nes.ppu.ppu_addr_mode ^= 1;

	  break;
	

      case ppu_data: /* 2007 */
	  if ((!in_vblank (&nes)) && (nes.ppu.control2 & 0x08)) {
	      nes_printtime ();
	      printf ("hblank wrote %02X\n", (int) Value);
	  } else {
              // TODO: Manual scanline twiddle for IRQ counter?

              if (nes.ppu.v >= 0x2000) {
                  if (nes.ppu.v < 0x3F00) {
                      if (nes.ppu.v < 0x3000) {	/* name/attribute table write */
                          int maddr = ppu_mirrored_nt_addr(nes.ppu.v);
                          
                          if (trace_ppu_writes) {
                              nes_printtime();
                              printf("ppu write: %04X (mirrored to %04X), wrote %02X, writemode=%i\n",
                                     nes.ppu.v, maddr, Value, nes.ppu.ppu_writemode);
                          }

                          nes.ppu.vram[maddr & 0x3FFF] = Value;
                      } else {
                          nes.ppu.vram[nes.ppu.v & 0x3FFF] = Value;
                          /* Technically this should mirror 0x2000, but I want
                           * to see a game require this before I enable it. */
                          printf ("ppu: Write to unused vram at 0x%04X\n", (int) nes.ppu.v);
                      }
                  } else {
                      /* Palette write */
                      word tmp = nes.ppu.v;
                      tmp &= 0x1F;
                      Value &= 63;
                      if (trace_ppu_writes) printf("%sPalette write of %02X to %04X (mirrored to %04X)\n", 
                                                   nes_time_string(), Value, nes.ppu.v, tmp);
                      catchup_emphasis();

                      if (!(tmp & 3)) {
                          nes.ppu.vram[0x3F00 | (tmp & 15)] = nes.ppu.vram[0x3F10 | (tmp | 16)] = Value;
                      } else nes.ppu.vram[0x3F00 | tmp] = Value;
                  }
              } else {
                  if (!nes.rom.chr_size) {
                      nes.ppu.vram[nes.ppu.v] = Value;
                  } /* else printf("PPU: attempted write into character ROM!\n"); */
              }
	  }
	  nes.ppu.v += nes.ppu.ppu_writemode;
	  nes.ppu.v &= 0x3FFF;
	  break;

      default:
	  printf ("Wr6502 is broken.\n");
	  break;
      }
      break;


  case 0x4000:
      if (Addr <= 0x4017) {
          if (Addr == 0x4014) {	/* sprite DMA transfer */
	  word i, tmp = nes.ppu.sprite_address;
          /* Sprite DMA should start from the current sprite ram address
             on the PPU. It seems unlikely that the CPU is designed to 
             first reset the address register. It costs 513 CPU cycles.
          */
          /* (If that's really the case, why not use it for other things?) */

          nes.cpu.Cycles += 513 * MASTER_CLOCK_DIVIDER;

          //printf("Sprite DMA from page %i to sprite+%02X\n", Value, tmp);
          for (i = 0; i < 256; i++) {
	    nes.ppu.spriteram[tmp] = Rd6502(0x100 * Value + i);
	    tmp++;
	    tmp&=0xFF;
	  }
	} else if (Addr < 0x4016) {	/* sound register write */
	  snd_write(Addr & 0x1F,Value);
	} else {		/* else joypad strobe or frameseq frobble */
	  joypad_write (Addr);
	  snd_write(Addr & 0x1F,Value);
	}
/*		  printf("sound: Wrote 0x%02X to sound register area at 0x%04X\n",(int)Value,(int)Addr); */
      } else if (Addr < 0x5000) {
	/*printf ("sound: Wrote 0x%02X to unknown area at 0x%04X\n", (int) Value, (int) Addr);*/
      } else {
	/*printf ("Wrote 0x%02X to expansion module area at 0x%04X\n", (int) Value, (int) Addr);*/
      }
      break;

  case 0x6000:    
      /* Some games (e.g. SMB3) need SRAM even though they don't back
       * it with a battery, and the rom flags don't reflect this, so
       * give them their ram by default. */
      nes.save[Addr & 0x1FFF] = Value;
      break;
    
  case 0x8000:
  case 0xA000:
  case 0xC000:
  case 0xE000:    
      mapper->mapper_write (Addr, Value);
      break;    
  }
}

byte Rd6502 (register word Addr);
byte InnerRd6502 (register word Addr);

#ifndef INSTRUCTION_TRACING
#define InnerRd6502 Rd6502
#endif

byte Op6502 (register word Addr)
{
#ifdef INSTRUCTION_TRACING
  tracing_counts[Addr][COUNT_EXEC]++;
#endif
  return InnerRd6502(Addr);
}


#ifdef INSTRUCTION_TRACING
byte Rd6502 (register word Addr)
{
  tracing_counts[Addr][COUNT_READ]++;
  return InnerRd6502(Addr);
}

byte InnerRd6502 (register word Addr)
#else 
byte Rd6502 (register word Addr)
#endif
{
  switch (Addr & 0xE000) {
  case 0x0000: return nes.ram[Addr & 0x07FF];
  case 0x2000:
      switch ((Addr & 0x0007) | 0x2000) {
      case ppu_cr1:
	//	return nes.ppu.control1;
      case ppu_cr2:
	//	return nes.ppu.control2;
      case ppu_addr:
      case spr_addr:
      case ppu_status:
      {
          /* A note about timing: The 6502 core increments cpu.Cycles
             *before* emulating the instruction, which is appropriate
             for timing-sensitive writes to e.g. $2001 (which should
             take effect *after* the write cycle), but for reading, we
             should reflect the state at the start of the read cycle
             itself. So, subtract one CPU clock when testing the time
             below. Nevertheless, there might be an off-by-1 type
             error here, because I haven't thought about the precise
             timing too hard (and the my timing isn't exact anyway).
           */
          if (nes.sprite0_detected && ((nes.cpu.Cycles - MASTER_CLOCK_DIVIDER - nes.sprite0_hit_cycle) > 0)) {
              nes.ppu.hit_flag = 1;
              nes.sprite0_detected = 0;
          }

	  byte tmp = 
              (nes.ppu.vblank_flag << 7) |
              (nes.ppu.hit_flag << 6) | 
              (nes.ppu.spritecount_flag << 5) |
              ((in_vblank(&nes))<<4); /* VRAM Write flag - this is a guess. ccovel's cmcwavy demo uses this. */

	  if (superverbose) {
	    if (nes.ppu.vblank_flag) {
	      nes_printtime ();
	      printf ("cleared vblank flag\n");
	    }	    
	    if (nes.ppu.hit_flag) {
	      nes_printtime ();
	      printf ("clear hit flag\n");
	    }
	  }

          /* Don't clear the PPU write mode! Breaks SMB2. */
          
	  nes.ppu.vblank_flag = 0;
	  /*	  nes.ppu.hit_flag = 0; */

	  nes.ppu.ppu_addr_mode = DUAL_HIGH;
	  nes.ppu.control1 &= (0xFF - 4);
	  return tmp;
      }

      case spr_data:
          /* I've read that OAM doesn't store the three unused sprite
           * attribute bits. I haven't verified this. */
          /* According to Quietust, these reads don't increment the address.*/
          return nes.ppu.spriteram[nes.ppu.sprite_address] & (((nes.ppu.sprite_address&3)==2)?0xE3:0xFF);

      case ppu_data:
      {
	  byte ret;
          
          /* Palette reads aren't buffered? */
          if (nes.ppu.v >= 0x3f00) nes.ppu.read_latch =  nes.ppu.vram[0x3f00 | (nes.ppu.v & 0x1f)];

          ret = nes.ppu.read_latch;
          

          nes.ppu.read_latch = nes.ppu.vram[ppu_mirrored_addr(nes.ppu.v & 0x3FFF)];
          // TODO: Manual scanline twiddle for IRQ counter?
          
          nes.ppu.v += nes.ppu.ppu_writemode;
          nes.ppu.v &= 0x7FFF;
          
	  return ret;
      }

      default:
	  printf ("PPU: Read from unknown ppu register 0x%04X\n", (int) Addr);
	  break;

      }
      break;      

  case 0x4000:
      if (Addr <= 0x4017) {
	switch (Addr) {
	case snd_status: return snd_read_status_reg();
	case 0x4016:
	case 0x4017: return joypad_read (Addr);
	default:
	    printf ("Read from unknown sound/input register 0x%04X\n", (int) Addr);
	    break;
	}
      } else {
          //printf ("Read from unknown/expansion area at 0x%04X\n", (int) Addr);
      }
      break;

  case 0x6000:
      return nes.save[Addr & 0x1FFF];

  case 0x8000:
  case 0xA000:
  case 0xC000:
  case 0xE000: return mapper->mapper_read(Addr);
  }

  return 0;
}

byte Debug6502 (register M6502 * R)
{
    char buffer[64];
    byte tmp[4];
    tmp[0] = Rd6502 (R->PC.W);
    tmp[1] = Rd6502 (R->PC.W + 1);
    tmp[2] = Rd6502 (R->PC.W + 2);
    tmp[3] = Rd6502 (R->PC.W + 3);
    DAsm (buffer, tmp, R->PC.W);
    nes_printtime ();
    printf ("$%04X  A=%02X X=%02X Y=%02X S=%02X  %17s   ", 
            (unsigned)R->PC.W, (unsigned)R->A, 
            (unsigned)R->X, (unsigned)R->Y, (unsigned)R->S,
            buffer);
    printf("\n");
    return 1;
}

static void run_until (int master_cycle)
{
    nes.cpu.BreakCycle = master_cycle;
    //printf("CPU to run until clock %u (%i cpu cycles)\n", nes.cpu.BreakCycle, (int)(nes.cpu.BreakCycle - nes.cpu.Cycles) / MASTER_CLOCK_DIVIDER);
    Run6502 (&nes.cpu);
}

static inline int ppu_is_rendering (void)
{
    return (nes.ppu.control2 & 0x18);
}

static inline void begin_scanline (void)
{
    nes.sprite0_detected = 0;
    emphasis_position = -1;
    memset(color_buffer, 0, sizeof(color_buffer));
    if (ppu_is_rendering()) rendering_scanline = 1;
}

static void finish_scanline_rendering (void)
{
    catchup_emphasis_to_x(256);
    filter_output_line(tv_scanline, color_buffer, emphasis_buffer);
    rendering_scanline = 0;

    /* Wrong: this should happen at PPU clock 260, not 256. Whatever. */
    if (ppu_is_rendering() && (nes.ppu.control1 & (BIT(3) | BIT(4)))) mapper_twiddle();
}

static inline void ppu_latch_v (void)
{
    if (ppu_is_rendering()) nes.ppu.v = (nes.ppu.v & ~0x41F) | (nes.ppu.t & 0x41F);
}

void nes_runframe (void)
{
    /* Cycle counts, in PPU color clocks.. */
    int line_cycles = 341;
    int hblank_cycles = 85;
    int scan_cycles = line_cycles - hblank_cycles;
    int vblank_kludge_cycles = 3 * MASTER_CLOCK_DIVIDER; /* ..except for this. */
    int vscroll;
    unsigned stripe_buffer[240];

    // Initialize for frame. Clears PPU flags.
    nes_initframe();

    //printf("Begin frame: scanline started at %u, CPU at %u\n", nes.scanline_start_cycle, nes.cpu.Cycles);

    /* Every other frame shortens this cycle by 1 PPU clock, to shift
     * the phase of the NTSC colorburst (?), if BG enabled. */
    if ((frame_number & 1) && (nes.ppu.control2 & 0x04)) nes.scanline_start_cycle -= PPU_CLOCK_DIVIDER;

    // Dummy scanline:
    begin_scanline();
    ppu_latch_v();
    render_scanline();
    run_until(nes.scanline_start_cycle + scan_cycles * PPU_CLOCK_DIVIDER);
    finish_scanline_rendering();
    run_until(nes.scanline_start_cycle + line_cycles * PPU_CLOCK_DIVIDER);
    nes.scanline_start_cycle += line_cycles * PPU_CLOCK_DIVIDER;    

    tv_scanline = 0;
    vscroll = ppu_current_vscroll() - 1;

    if (0 && trace_ppu_writes)
        printf("At frame start, hscroll=%03i vscroll=%03i\n", 
               ppu_current_hscroll(), ppu_current_vscroll());

    // This is the visible frame:
    for (nes.scanline = 1; nes.scanline < 241; nes.scanline++)
    {
        begin_scanline();
        ppu_latch_v();

        if (trace_ppu_writes && (nes.ppu.control2 & 0x08) && (ppu_current_vscroll() != vscroll+1)) {
            nes_printtime();
            printf("split detected. %03i/%03i\n", ppu_current_hscroll(), ppu_current_vscroll());
        }
        vscroll = ppu_current_vscroll();

        render_scanline();
        stripe_buffer[tv_scanline] = rgb_palette[color_buffer[video_stripe_idx] & 63];
        if (video_stripe_output) color_buffer[video_stripe_idx] ^= 0x34;

        run_until(nes.scanline_start_cycle + scan_cycles * PPU_CLOCK_DIVIDER);
        finish_scanline_rendering();
        run_until(nes.scanline_start_cycle + line_cycles * PPU_CLOCK_DIVIDER);
        nes.scanline_start_cycle += line_cycles * PPU_CLOCK_DIVIDER;
        snd_catchup();
        tv_scanline++;
    }

    // If the video stripe recorder is going, write out the buffer:
    if (video_stripe_output) {
        fwrite(stripe_buffer, sizeof(stripe_buffer), 1, video_stripe_output);
    }

    // One wasted line after the frame:
    begin_scanline();
    run_until(nes.scanline_start_cycle + line_cycles * PPU_CLOCK_DIVIDER);
    nes.scanline_start_cycle += line_cycles * PPU_CLOCK_DIVIDER;
    nes.scanline++;

    // Enter vertical blank
    nes_vblankstate();
    run_until(nes.scanline_start_cycle + vblank_kludge_cycles);
    if (nes.ppu.control1 & 0x80) Int6502(&nes.cpu, INT_NMI);
    for (; nes.scanline < 262; nes.scanline++) {
        begin_scanline();
        run_until(nes.scanline_start_cycle + line_cycles * PPU_CLOCK_DIVIDER - vblank_kludge_cycles);
        nes.scanline_start_cycle += line_cycles * PPU_CLOCK_DIVIDER;
        vblank_kludge_cycles = 0;
    }

    snd_catchup();
}



/* Debugging utilities: The idea is that rather than writing a 6502
 * debugger as part of the emulator, we can debug from the host's GDB
 * using watchpoints and inspecting the emulator data structures
 * directly. These are here to aid in that approach (e.g. "display
 * list()"). In practice this is a pain in the ass, but it has worked
 * well enough so far.
 */

word getword (word addr)
{
    //return ((int)mapper->mapper_read(addr+1)<<8) + mapper->mapper_read(addr);
    return ((int)Rd6502(addr+1)<<8) + Rd6502(addr);
}

void vectors (void)
{
    printf("Interrupt vectors:\n");
    printf(" IRQ - %04X\n", getword(0xFFFE));
    printf(" RST - %04X\n", getword(0xFFFC));
    printf(" NMI - %04X\n", getword(0xFFFA));
}

void regs (void)
{
    printf("PC=%04X  A=%02X  X=%02X Y=%02X  P=%02X S=%02X\n", 
           nes.cpu.PC.W, nes.cpu.A, nes.cpu.X, nes.cpu.Y, nes.cpu.P, nes.cpu.S);
}

void curinstr (void)
{
    char buf[256];
    byte instr[8];
    int i;
    for (i=0; i<8; i++) instr[i] = Rd6502((nes.cpu.PC.W + i) & 0xFFFF);
    DAsm(buf, instr, nes.cpu.PC.W);
    printf("%04X: %s\n", nes.cpu.PC.W, buf);
}

void list (void)
{
    char buf[256];
    byte instr[8];
    int i;
    for (i=0; i<8; i++) instr[i] = Rd6502((nes.cpu.PC.W + i) & 0xFFFF);
    DAsm(buf, instr, nes.cpu.PC.W);
    printf("PC=%04X  A=%02X  X=%02X Y=%02X  fl=%02X S=%02X   %s\n", 
           nes.cpu.PC.W, nes.cpu.A, nes.cpu.X, nes.cpu.Y, nes.cpu.P, nes.cpu.S, buf);
}

void note_brk (void)
{
    if (debug_brk) {
        byte code = Rd6502(nes.cpu.PC.W);
        printf("%sBRK %02X: ", nes_time_string(), code); 
        regs();
        /* The MSB of the break code enables instruction tracing */
        nes.cpu.Trace = code >> 7;
    }
}
