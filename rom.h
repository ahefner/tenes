#ifndef ROM_H
#define ROM_H

#include "M6502/M6502.h"
#include "mapper_info.h"

#define MIRROR_HORIZ 0
#define MIRROR_VERT  1
#define MIRROR_NONE  2  /* 4-screen VRAM */
#define MIRROR_ONESCREEN 3

struct nes_rom
{
    byte *chr;
    byte *prg;
    byte save[0x2000];
    unsigned prg_size;
    unsigned chr_size;
    int mapper;
    char title[256];
    char filename[256];
    byte flags; /* mirroring, etc. */
    int mirror_mode;
    int onescreen_page;
    struct mapperinfo *mapper_info;
    byte header[16];
    unsigned long long hash;    
};

struct nes_rom load_nes_rom (char *filename);
void free_rom(struct nes_rom *rom);
void save_sram(struct nes_rom *rom, int verbose);
	
#endif
