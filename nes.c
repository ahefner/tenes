
#define NES_C

#include <assert.h>
#include "sys.h"
#include "nes.h"
#include "sound.h"
#include "dasm.c"
#include "vid.h"
#include "config.h"
#include "global.h"

int waitingforhit;

/* init_nes - initializes a nes_machine struct that already had it's rom member set properly */
/*            after this, an nes_reset is all it should take to start the first frame.       */
void init_nes (struct nes_machine *nes)
{
  int i, idx = -1;
  struct mapper_functions *info = NULL;

#ifdef INSTUCTION_TRACING
  memset(tracing_counts,0,sizeof(tracing_counts));
#endif

  if (forcemapper!=-1) {
    printf ("Forcing mapper to %i\n", forcemapper);
    nes->rom.mapper = forcemapper;
  }

  for (i = 0; i < MAPTABLESIZE; i++) {
    if (MapTable[i].id == nes->rom.mapper) {
      idx = i;
      info = MapTable[i].info;
    }
  }
  if (idx != -1) {
    printf ("Mapper is \"%s\"\n", MapTable[idx].name);
  } else {
    printf ("Unknown mapper (%i).\n", nes->rom.mapper);
  }
  if (info == NULL) {
    printf ("Defaulting to mapper 0.\n");
    info = MapTable[0].info;
  }
  nes->joypad.connected = 1;

  nes->mapper = info;
  nes->mapper->mapper_init ();
  printf ("NES initialized.\n");
}

void shutdown_nes (struct nes_machine *nes)
{
  nes->mapper->mapper_shutdown ();
}

/* reset_nes - resets the state of the cpu, ppu, sound, joypads, and internal state */
void reset_nes (struct nes_machine *nes)
{
  memset ((void *) nes->ram, 0, 0x800);
  Reset6502 (&nes->cpu);
  if (cfg_trapbadops) nes->cpu.TrapBadOps = 1;
  else nes->cpu.TrapBadOps = 0;

  memset ((void *) (nes->ppu.vram + 0x2000), 0, 0x2000);
  nes->ppu.control1 = 0;
  nes->ppu.control2 = 0;
  nes->ppu.hscroll = 0;
  nes->ppu.vscroll = 0;
  nes->ppu.sprite_address = 0;
  nes->ppu.ppu_address = 0;
  nes->ppu.read_latch = 0;
  nes->ppu.ppu_addr_mode = DUAL_HIGH;
  nes->ppu.ppu_writemode = PPUWRITE_HORIZ;	/* this is a guess */
  /*nes->ppu.scroll_mode = DUAL_HIGH;*/	/* high meaning horizontal first */
  nes->ppu.vblank_flag = 0;

  memset ((void *) nes->joypad.pad, 0, 4 * 8 * sizeof (int));
  nes->joypad.state[0] = 0;
  nes->joypad.state[1] = 0;

  memset ((void *) &nes->snd, 0, sizeof (nes->snd));
  printf ("NES reset.\n");
}

/* nes_initframe - call at the start of every frame */
void nes_initframe (struct nes_machine *nes)
{
  nes->ppu.hit_flag = 0;	/* set to 0 at frame start */
  nes->ppu.vblank_flag=0; /* set to 0 at frame start, I think */
  nes->ppu.hbwrite_mode=DUAL_HIGH; /* set this at the start of every frame */
  nes->ppu.hbwrite_count = 0;	/* "                                  " */
  nes->ppu.spritecount_flag = 0;
  nes->ppu.sprite_address = 0;
  waitingforhit = 1;

  nes->scanline = 0; 
}

/* nes_vblankstate - sets things that change when a vblank occurs */
void nes_vblankstate (struct nes_machine *nes)
{
  /*   nes->ppu.scroll_mode = DUAL_HIGH; */ /* ??? */
  nes->ppu.vblank_flag = 1;
  nes->ppu.hit_flag = 0;
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
    retval = nes.joypad.pad[padindex][state];
  } else if (state < 16) {
    retval = nes.joypad.pad[padindex + 2][state - 8];
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

void nes_printtime (void)
{
  printf ("%i.%i ", nes.scanline, (int) nes.cpu.IPeriod - nes.cpu.ICount);
}

void mapper_twiddle (void)
{
    if (nes.mapper->scanline()) Int6502(&nes.cpu, INT_IRQ);
}

/*  CPU/Hardware interface.  */

inline int in_vblank (struct nes_machine *nes)
{
    return (nes->scanline >= cfg_framelines) ? 1 : 0;
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
      switch ((Addr & 0x0007) | 0x2000) {

      case ppu_cr1: /* 2000 */

	  nes.ppu.control1 = Value;
	  nes.ppu.ppu_writemode = (Value & 0x04) ? PPUWRITE_VERT : PPUWRITE_HORIZ;
	  
	  if (superverbose) 
	    {
	      nes_printtime ();
              printf("PPU 2000: ");
	      PrintBin (Value);
	      printf(" next PC=$%04X, ",nes.cpu.PC.W);	      
	      if (Value & 0x80) printf ("NMI ON  ");
	      if (Value & 0x10) printf ("bg1 ");
	      else printf ("bg0 ");
	      if (Value & 0x08) printf ("spr1 ");
	      else printf ("spr0 ");
	      /*                 if(Value & 0x20) printf("8x16 sprites "); else printf("8x8 sprites "); */
	      if (Value & 0x04) printf ("Vertical write\n");
	      else printf ("Horiz write.\n");
	      /*                 if(!in_vblank(&nes)) printf("PPU: CR1 written during frame\n"); */
	      printf("\n");
	    }
	  break;

	
      case ppu_cr2: /* 2001 */
	  nes.ppu.control2 = Value;
	  if (superverbose) {
            nes_printtime ();
             printf("PPU 2001: ");
	    if (Value & 0x10) printf ("sprites ON  ");
	    else printf ("sprites OFF  ");
	    if (Value & 0x08) printf ("bg ON  ");
	    else printf ("bg OFF  ");
	    /*                 if(Value & 0x01) printf("MONO display mode\n"); else printf("Color display\n"); */
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
          printf("Sprite[%i] = %i\n", (unsigned)nes.ppu.sprite_address, (unsigned)Value);
	  nes.ppu.spriteram[nes.ppu.sprite_address++] = Value;
          nes.ppu.sprite_address &= 0xFF;
	  break;


      case ppu_bgscroll: /* 2005 */
	  //	  nes_printtime();
	  //	  printf ("%cscroll=%02X\n", nes.ppu.ppu_addr_mode == DUAL_HIGH?'h':'v', (unsigned)Value);
	  if (superverbose) /* loopy says that $2005 and $2006 share the same toggled state */
	    nes_printtime ();
	  if (nes.ppu.ppu_addr_mode == DUAL_HIGH) {
	    if (superverbose) printf ("hscroll=%u\n", (unsigned) Value);
	    nes.ppu.ppu_addr_mode = DUAL_LOW;
	    nes.ppu.hscroll = Value;
	  } else {
	    if (superverbose) printf ("vscroll=%u\n", (unsigned) Value);
	    nes.ppu.ppu_addr_mode = DUAL_HIGH;
	    if (Value<=239) nes.ppu.vscroll = Value;
	  }
	  break;


      case ppu_addr: /* 2006 */
	  //	  nes_printtime();
          //printf ("ppu_addr, wrote %02X %s\n", (unsigned) Value, (nes.ppu.ppu_addr_mode==DUAL_HIGH?"high":"low"));

	  if (nes.ppu.ppu_addr_mode == DUAL_HIGH) {
	    nes.ppu.ppu_addr_mode = DUAL_LOW;
	    nes.ppu.ppu_address = (nes.ppu.ppu_address & 0x00FF) | (Value << 8);
	  } else {
	    nes.ppu.ppu_addr_mode = DUAL_HIGH;
	    nes.ppu.ppu_address = (nes.ppu.ppu_address & 0xFF00) | Value;
	  }
	  nes.ppu.ppu_address &= 0x3FFF;
          if (trace_ppu_writes)
              printf("%u.%u: ppu addr = %04X\n", frame_number, nes.scanline, nes.ppu.ppu_address);
          
          /* I'd like to emulate manual cycling of the MMC3 IRQ counter via PPU A12, but I'm not sure how it should work. In particular, it doesn't make any sense to me that a write to this register should */
          // TODO: Manual scanline twiddle for IRQ counter?


/*		       printf("ppu_addr = %04X\n",nes.ppu.ppu_address); */
	  break;
	

      case ppu_data: /* 2007 */

	  if ((!in_vblank (&nes)) && (nes.ppu.control2 & 0x08)) {

	    /*if (superverbose)*/ {
	      nes_printtime ();
	      printf ("hblank wrote %02X ", (int) Value);
	      if (!(nes.ppu.control2 & 0x08))
		printf ("\tBG OFF");
	      printf ("\n");
	    }

	    if (nes.ppu.hbwrite_mode == DUAL_HIGH) {
	      nes.ppu.hbwrite_mode = DUAL_LOW;
	      nes.ppu.hbwrite.val = Value << 8;
	    } else {
	      nes.ppu.hbwrite_mode = DUAL_HIGH;
	      nes.ppu.hbwrite.val = nes.ppu.hbwrite.val | Value;
	    }
	    nes.ppu.hbwrite_count++;
	  } else {
/*			    if((nes.ppu.control2&0x08)) 
			      {
				 nes_printtime();
				 printf("ppu wrote %02X",(int)Value);
				 if(!(nes.ppu.control2&0x08)) printf("\tBG OFF");
				 printf("\n");
			      }*/
              // TODO: Manual scanline twiddle for IRQ counter?

	    if (nes.ppu.ppu_address >= 0x2000) {
	      if (nes.ppu.ppu_address < 0x3F00) {
		if (nes.ppu.ppu_address < 0x3000) {	/* name/attribute table write */
                    int maddr = ppu_mirrored_nt_addr(nes.ppu.ppu_address);

                    if (trace_ppu_writes)
                        printf("%u.%u: ppu write: %04X (mirrored to %04X), wrote %02X, writemode=%i\n",
                               frame_number, nes.scanline, 
                               nes.ppu.ppu_address, maddr, Value, nes.ppu.ppu_writemode);

                    nes.ppu.vram[maddr] = Value;
		} else {
		  nes.ppu.vram[nes.ppu.ppu_address] = Value;
                  /* Technically this should mirror 0x2000, but I want
                   * to see a game require this before I enable it. */
		  printf ("ppu: Write to unused vram at 0x%04X\n", (int) nes.ppu.ppu_address);
		}
	      } else {		/* palette write - palette mirroring must be done in read function */		
		word tmp = nes.ppu.ppu_address;
                //printf("pal %X %i\n", tmp, Value);
                //printf("IRQ vector is %X\n", Rd6502(0xFFFE) +  (Rd6502(0xFFFF)<<8));
                //Debug6502(&nes.cpu);

		vid_tilecache_dirty = 1;
		tmp &= 0x1F;
		tmp |= 0x3F00;
/*				      if(!(tmp&0x3)) 
					{
					   int i;
					   for(i=0x3F00;i<=0x3F20;i+=4) nes.ppu.vram[i]=Value;
					}*/
		if ((tmp == 0x3F00) || (tmp == 0x3F10)) {
		  nes.ppu.vram[0x3F00] = Value;
		} else {	/* else normal palette write */
		  nes.ppu.vram[tmp] = Value;
		}
	      }
	    } else {
	      if (!nes.rom.chr_size) {
		nes.ppu.vram[nes.ppu.ppu_address] = Value;
		vid_tilecache_dirty = 1;
	      }
	      /*printf("PPU: attempted write into character ROM!\n"); */
	    }
	  }
	  nes.ppu.ppu_address += nes.ppu.ppu_writemode;
	  nes.ppu.ppu_address &= 0x3FFF;
	  break;

      default:
	  printf ("Wr6502 is broken for registers.\n");
	  break;
      }
      break;


  case 0x4000:
      if (Addr <= 0x4017) {
          if (Addr == 0x4014) {	/* sprite DMA transfer */
	  word i, tmp = nes.ppu.sprite_address;
          /* Sprite DMA should start from the current sprite ram address
             on the PPU. It seems unlikely that the CPU is designed to 
             first reset the address register. */
	  // for (i = 0x100 * Value; i < (0x100 * Value + 0x100); i++) {
          //printf("Sprite DMA from page %i to sprite+%02X\n", Value, tmp);
          for (i = 0; i < 256; i++) {
	    nes.ppu.spriteram[tmp] = Rd6502 (0x100 * Value + i);
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
      nes.rom.save[Addr & 0x1FFF] = Value;
      break;
    
  case 0x8000:
  case 0xA000:
  case 0xC000:
  case 0xE000:    
      nes.mapper->mapper_write (Addr, Value);
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
  case 0x0000:
    return nes.ram[Addr & 0x07FF];
  case 0x2000:
      switch ((Addr & 0x0007) | 0x2000) {
      case ppu_cr1:
	//	return nes.ppu.control1;
      case ppu_cr2:
	//	return nes.ppu.control2;
      case ppu_addr:
      case spr_addr:
      case spr_data:
      case ppu_status:
	{
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
	  /*	  nes.ppu.scroll_mode= DUAL_HIGH;*/
	  nes.ppu.ppu_addr_mode = DUAL_HIGH;
	  nes.ppu.control1 &= (0xFF - 4);
	  return tmp;
	}
      case ppu_data:
	{
	  byte ret = nes.ppu.read_latch;

          nes.ppu.read_latch = nes.ppu.vram[ppu_mirrored_addr(nes.ppu.ppu_address)];
          // TODO: Manual scanline twiddle for IRQ counter?
          
          nes.ppu.ppu_address += nes.ppu.ppu_writemode;
          nes.ppu.ppu_address &= 0x3FFF;
          
	  return ret;
	}

      default:
	{
	  printf ("PPU: Read from unknown ppu register 0x%04X\n", (int) Addr);
	  break;
	}
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
      return nes.rom.save[Addr & 0x1FFF];

  case 0x8000:
  case 0xA000:
  case 0xC000:
  case 0xE000: return nes.mapper->mapper_read (Addr);  
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
  printf ("PC $%04X %s \tA=%02X X=%02X Y=%02X S=%02X\n", 
          (unsigned)R->PC.W, buffer, 
          (unsigned)R->A, (unsigned)R->X, (unsigned)R->Y, (unsigned)R->S);
  return 1;
}




byte Loop6502 (register M6502 * R)
{
  byte ret = INT_NONE;

  /*  if (superverbose) printf("scanline %i\n",nes.scanline);  */
  if (nes.scanline == nes.ppu.spriteram[0]) {
      nes.ppu.hit_flag = 1;
    if (superverbose) printf ("Sprite 0 hit on line %i (s0y=%i, s0t=%i)\n", (int)nes.scanline, (int)nes.ppu.spriteram[0],(int)nes.ppu.spriteram[1]);
  } 
  
  /* This is more correct, but there's a bug in it... */
  /*  if ( (nes.scanline>=nes.ppu.spriteram[0]) && (((int)nes.scanline)<(((int)nes.ppu.spriteram[0])+8))) {
    unsigned *chr = (unsigned *) ((unsigned char *)nes.ppu.vram + ((nes.ppu.control1 & 8)<<9));
    int i = 0;
    chr += 16 * nes.ppu.spriteram[1];
    while (i <= (nes.scanline - nes.ppu.spriteram[0])) {
      if (chr[i*2+0] || chr[i*2+1]) {
	nes.ppu.hit_flag = 1;
	waitingforhit = 0;
	printf ("Sprite 0 hit on line %i (s0y=%i, s0t=%i)\n", (int)nes.scanline, (int)nes.ppu.spriteram[0],(int)nes.ppu.spriteram[1]);
	break;
      } else i++;
    }
    }*/
      

  if (nes.scanline == 0) nes.ppu.vblank_flag = 0;

  if ((nes.scanline<256)) {
    lineinfo[nes.scanline].hscroll = nes.ppu.hscroll;
    lineinfo[nes.scanline].vscroll = nes.ppu.vscroll;
    lineinfo[nes.scanline].control1 = nes.ppu.control1;
    lineinfo[nes.scanline].control2 = nes.ppu.control2;
  } 

  if (nes.scanline == cfg_framelines) {
    nes_vblankstate (&nes);
    if (nes.ppu.control1 & 0x80) ret = INT_NMI; /* trigger VBlank NMI if enabled in PPU */
  } else if (nes.scanline == (cfg_framelines + cfg_vblanklines - 1))
    ret = INT_QUIT;

/*   printf("line %i  vblank=%i flag=%i ppu_addr=%04X\n",nes.scanline,in_vblank(&nes),nes.ppu.vblank_flag,(int)nes.ppu.ppu_address); */  
  nes.scanline++;

  if ((!in_vblank(&nes)) && (nes.ppu.control1 & (BIT(3) | BIT(4)))) mapper_twiddle();

  return ret;
}

void nes_runframe (void)
{
  nes.cpu.IPeriod = cfg_linecycles;
  nes_initframe (&nes);
  if (superverbose) {
    printf ("ppu @ $%04X\n", (unsigned)nes.ppu.ppu_address);
  }
  Run6502 (&nes.cpu);
  if (superverbose)
    printf ("\n\n\n");
  snd_frameend ();

  if (superverbose) {
    printf("ppu=%X\n",nes.ppu.control1&0x40); 
    printf("nametable=%i\n",nes.ppu.control1&3);
  }
}

/** Debugging utilities: The idea is that rather than writing a 6502
 * debugger as part of the emulator, we can debug from the host's GDB
 * using watchpoints and inspecting the emulator data structures
 * directly. These are here to aid in that approach (e.g. "display
 * list()")
 **/

word getword (word addr)
{
    //return ((int)nes.mapper->mapper_read(addr+1)<<8) + nes.mapper->mapper_read(addr);
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
