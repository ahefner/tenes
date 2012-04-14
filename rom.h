#ifndef ROM_H
#define ROM_H

#include "M6502/M6502.h"
#include "mapper_info.h"

struct nes_rom load_nes_rom (const char *filename);
void free_rom(struct nes_rom *rom);
void save_sram(byte save[0x2000], struct nes_rom *rom, int verbose);

#endif
