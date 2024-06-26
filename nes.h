#ifndef NES_H
#define NES_H

#include <assert.h>
#include "M6502/M6502.h"
#include "global.h"
#include "rom.h"

#define PPU_CLOCK_DIVIDER (MASTER_CLOCK_DIVIDER/3)

/***  PPU registers  ***/

#define ppu_cr1         0x2000
#define ppu_cr2         0x2001
#define ppu_status      0x2002
#define ppu_bgscroll    0x2005   /* double write */
#define ppu_addr        0x2006   /* double write */
#define ppu_data        0x2007   /* RW */


/** Sprite registers **/

#define spr_addr   0x2003
#define spr_data   0x2004        /* RW */
#define spr_dma    0x4014

/**  Control pad registers  **/

#define joypad_1   0x4016        /* RW */
#define joypad_2   0x4017        /* RW */

/***  Sound registers (pAPU) ***/

#define snd_p1c1  0x4000 /* Pulse channel 1 */
#define snd_p1c2  0x4001
#define snd_p1f1  0x4002
#define snd_p1f2  0x4003

#define snd_p2c1  0x4004 /* Pulse channel 2 */
#define snd_p2c2  0x4005
#define snd_p2f1  0x4006
#define snd_p2f2  0x4007

#define snd_tric1 0x4008 /* Triangle channel */
#define snd_tric2 0x4009
#define snd_trif1 0x400A
#define snd_trif2 0x400B

#define snd_nc1   0x400C /* Noise channel */
#define snd_nc2   0x400D
#define snd_nf1   0x400E
#define snd_nf2   0x400F

#define snd_dmc_control  0x4010 /* DMC (PCM) channel */
#define snd_dmc_volume   0x4011
#define snd_dmc_address  0x4012
#define snd_dmc_datalen  0x4013

#define snd_status 0x4015 /* status/control register */


/* Hardware abstractions */

#define PPUWRITE_HORIZ	1   /* VRAM write mode - horizontal or vertical */
#define PPUWRITE_VERT	32

#define DUAL_HIGH	0       /* dual write register mode */
#define DUAL_LOW	1

#define MIRROR_HORIZ 0
#define MIRROR_VERT  1
#define MIRROR_NONE  2  /* 4-screen VRAM */
#define MIRROR_ONESCREEN 3

typedef int dualmode;

enum machine_type { NES_NTSC = 1, NSF_PLAYER };

struct nsf_header {
    byte magic[5];
    byte version;
    byte total_songs;
    byte starting_song;
    word load_addr, init_addr, play_addr;
    char name[32];
    char artist[32];
    char copyright[32];
    word speed_ntsc;
    byte bankswitch[8];
    word speed_pal;
    byte pal_mode;
    byte chipflags;
    byte unused[4];
};

/* NES Rom image. Note that this is the only portion of the
 * nes_machine structure not touched by save/restore state. */
struct nes_rom
{
    byte *chr;
    byte *prg;
    unsigned prg_size;
    unsigned chr_size;
    int mapper;
    char title[256];
    char filename[256];
    byte flags; /* mirroring, etc. */
    int hw_mirror_mode;            /* Hardwired / initial value */
    int hw_onescreen_page;         /* Hardwired / initial value */
    struct mapperinfo *mapper_info;
    byte header[16];            /*  iNES header */
    unsigned long long hash;
    enum machine_type machine_type;
    struct nsf_header *nsf_header; /* This points to static data. */
};

struct ppu_struct
{
    byte vram[0x4000];
    byte spriteram[0x100];
    byte sprite_address;

    word v, t;
    byte x;

    dualmode ppu_addr_mode;
    int ppu_writemode; /* horizontal or vertical write pattern */

    byte read_latch;

    byte control1,control2;

    int vblank_flag;
    int hit_flag;
    int spritecount_flag; /* more than 8 sprites on current scanline ? */
};

typedef float sfloat_t;

struct sound_struct
{
    byte regs[0x17];

    int ptimer[5];;   /* 11 bit down counter */
    int wavelength[5]; /* controls ptimer */
    int frameseq_divider; //frameseq_divider_reset;
    int frameseq_sequencer;
    int frameseq_mode_5;
    int frameseq_irq_disable;
    int out_counter[3]; /* for square and triangle channels.. */
    int lcounter[4]; /* 7 bit down counter */
    int sweep_reset[2];
    int sweep_divider[2];
    int envc_divider[4];
    int envc[4];
    int env_reset[4];
/* We latch the output of the envelope/constant volume here rather
 * than having to branch on the envelope disable flag for every sample
 * of output. */
    int volume[4];
    int linear_counter;
    int linear_counter_halt;
    unsigned dmc_address;
    unsigned dmc_counter;
    byte dmc_buffer;
    int dmc_dac;
    int dmc_shift_counter;
    int dmc_shift_register;
    sfloat_t filter_accumulator;
};

struct joypad_info
{
   byte pad[4];
   int connected;
   int state[2];
};

struct nes_machine
{
    struct nes_rom rom;
    M6502 cpu;
    struct ppu_struct ppu;
    struct sound_struct snd;
    byte ram[0x800];  /* first 8 pages, mirrored 4 times to fill to 0x1FFF */
    byte save[0x2000];          /* Save RAM, if present. */
    int scanline;
    int time;

    int mirror_mode, onescreen_page; /* Copy from ROM at reset. Some mappers change these. */

    long long last_sound_cycle; /* Last CPU cycle at sound was updated */
    long long scanline_start_cycle;

    struct joypad_info joypad;

    enum machine_type machine_type;
    int nsf_current_song;  /* Zero-based, as it should be. */
};

#ifndef global_c
extern
#endif
struct mapper_methods *mapper;

#ifndef global_c
extern
#endif
struct nes_machine nes;

void init_nes (struct nes_machine *nes);
void shutdown_nes (struct nes_machine *nes);
void hard_reset_nes (struct nes_machine *nes);
void soft_reset_nes (struct nes_machine *nes);
int open_game (const char *filename);
void close_current_game (void);
void nes_emulate_frame(void);

void nsf_emulate_frame(void);
void nsf_load_bank (unsigned frame, unsigned bank);

const char *nes_time_string (void);
void nes_printtime (void);

void save_state_to_disk (const char *filename);
int restore_state_from_disk (const char *filename);

#define MEMSTATE_MAX_CHUNKS 32
struct saved_state {
    int num, cur;
    void *chunks[MEMSTATE_MAX_CHUNKS];
    size_t lengths[MEMSTATE_MAX_CHUNKS];
};

void free_saved_chunks (struct saved_state *state);
void copy_saved_chunks (struct saved_state *dst, struct saved_state *src);

int mem_write_state_chunk (struct saved_state *state, const char *name, void *data, unsigned length);
int mem_read_state_chunk  (struct saved_state *state, const char *name, void *data_out, unsigned length);

void save_state_to_mem (struct saved_state *state);
void restore_state_from_mem (struct saved_state *state);

int file_write_state_chunk (FILE *stream, const char *name, void *data, unsigned length);
int file_read_state_chunk  (FILE *stream, const char *name, void *data_out, unsigned length);

const char *sram_filename (struct nes_rom *rom);
const char *state_filename (struct nes_rom *rom, unsigned index);

static inline word ppu_mirrored_nt_addr (word paddr)
{
    //word paddr_hbit = (paddr & 0x400) >> 10;
    word paddr_vbit = (paddr & 0x800) >> 11;

    switch (nes.mirror_mode) {

    case MIRROR_HORIZ:
        return (paddr & 0xF3FF) | (paddr_vbit << 10);

    case MIRROR_VERT:
        return paddr & 0xF7FF;

    case MIRROR_NONE:
        return paddr;

    case MIRROR_ONESCREEN:
        return (paddr & ~0xC00) | nes.onescreen_page;

    default:
        assert(0);
        return 0x2000;
    }
}

static inline word ppu_mirrored_addr (word paddr)
{
    paddr &= 0x3FFF;
    if ((paddr >= 0x2000) && (paddr < 0x3000)) return ppu_mirrored_nt_addr(paddr);
    else if (paddr >= 0x3F00) {
        paddr &= 0x3F1F;
        if (paddr == 0x3F10) return 0x3F00;
        else return paddr;
    } else return paddr;
}

/// Used by video filters to swizzle output pixels
struct rgb_shifts
{
    byte r_shift, g_shift, b_shift;
};

#define APU_LOG_LENGTH 32
struct apulog
{
    unsigned frame_count;
    FILE *out;
    int log_index;
    byte regnum[APU_LOG_LENGTH];
    byte value[APU_LOG_LENGTH];
//struct apuwrite frame_writes[APU_LOG_LENGTH];
};

void apulog_start_frame (struct apulog *log);
void apulog_write (struct apulog *log, word Addr, byte Value);
void apulog_end_frame (struct apulog *log);

#endif
