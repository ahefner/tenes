/* NES mappers */

#define MAPPER_INFO_C

#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "mapper_info.h"
#include "nes.h"
#include "vid.h"

/***********************************************************************
 * Helper functions for mapper implementation                          */

void mapper_select_chr_page (byte page, byte Value)
{
    unsigned tmp = Value*0x1000;
    if (tmp > (nes.rom.chr_size-0x1000)) tmp %= nes.rom.chr_size;
    assert(tmp <= nes.rom.chr_size-0x1000);
    memcpy((void *)nes.ppu.vram + (page&1)*0x1000,(void *)(nes.rom.chr+tmp),0x1000);
}


/* Mapper implementations */

#include "mappers/base.c"
#include "mappers/mmc1.c"
#include "mappers/vromswitch.c"
#include "mappers/konami2.c"
#include "mappers/mmc3.c"
#include "mappers/axrom.c"
#include "mappers/camerica.c"
#include "mappers/vrc6.c"
#include "mappers/nsf.c"
#include "mappers/gxrom.c"

/***********************************************************************
 * Mapper table - contains iNES mapper number, name, and pointer to
 * mapper_methods struct (if implemented).
 */

struct mapperinfo MapTable[] = {

  {0,  "None", &mapper_None},
  {1,  "MMC1", &mapper_MMC1},
  {2,  "Konami 74HC161/74HC32", &mapper_konami},
  {3,  "VROM Switch", &mapper_VROM},
  {4,  "MMC3", &mapper_MMC3},
  {5,  "MMC5", NULL},
  {6,  "Front Far East F4", NULL},
  {7,  "Rom Switch", &mapper_axrom},
  {8,  "Front Far East F3", NULL},
  {9,  "MMC2", NULL},
  {10, "MMC4", NULL},
  {11, "Color Dreams Mapper", NULL},
  {12, "Front Far East F6", NULL},
  {13, "Unknown", NULL},
  {14, "Unknown", NULL},
  {15, "100-in-1", NULL},
  {16, "Bandai", NULL},
  {17, "Front Far East F8", NULL},
  {18, "Jaleco SS8806", NULL},
  {19, "Namcot 106", NULL},
  {20, "Famicom Disk System", NULL},
  {21, "Konami VRC4", NULL},
  {22, "Konami VRC2 v1", NULL},
  {23, "Konami VRC2 v2", NULL},
  {24, "Konami VRC6", &mapper_vrc6},
  {25, "Unknown", NULL},
  {26, "Unknown", NULL},
  {27, "Unknown", NULL},
  {28, "Unknown", NULL},
  {29, "Unknown", NULL},
  {30, "Unknown", NULL},
  {31, "Unknown", NULL},
  {32, "Irem G-101", NULL},
  {33, "Taito TC0190/TC0350", NULL},
  {34, "iNES Mapper #34", NULL},
  {66, "GxROM", &mapper_GxROM},
  {71, "Camerica", &mapper_camerica}};

static int mapper_table_size (void)
{
    return sizeof(MapTable) / sizeof(MapTable[0]);
}

struct mapperinfo *
mapper_find (int ines_number)
{
    int i;
    for (i=0; i<mapper_table_size(); i++)
        if (MapTable[i].id == ines_number) return MapTable+i;

    return NULL;
}

static struct mapperinfo minf_NSF = {0, "NSF Player", &mapper_NSF};

struct mapperinfo *get_NSF_minf (void)
{
    return &minf_NSF;
}
