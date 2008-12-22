/* NES mappers */

#define MAPPER_INFO_C

#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "mapper_info.h"
#include "nes.h"
#include "vid.h"

#include "mappers/base.c"
#include "mappers/mmc1.c"
#include "mappers/vromswitch.c"
#include "mappers/konami2.c"
#include "mappers/mmc3.c"

/* MapTable - contains mapper id/support level
   Entry in maptable should match iNes Mapper # (id) */

struct mapperinfo MapTable[MAPTABLESIZE] = {

  {0, "None", 1, &mapper_None},
  {1, "MMC1", 1, &mapper_MMC1},
  {2, "Konami 74HC161/74HC32", 1, &mapper_konami},
  {3, "VROM Switch", 1, &mapper_VROM},
  {4, "MMC3", 0, &mapper_MMC3},
  {5, "MMC5", 0, NULL},
  {6, "Front Far East F4", 0, NULL},
  {7, "Rom Switch", 0, NULL},
  {8, "Front Far East F3", 0, NULL},
  {9, "MMC2", 0, NULL},
  {10, "MMC4", 0, NULL},
  {11, "Color Dreams Mapper", 0, NULL},
  {12, "Front Far East F6", 0, NULL},
  {13, "Unknown", 0, NULL},
  {14, "Unknown", 0, NULL},
  {15, "100-in-1", 0, NULL},
  {16, "Bandai", 0, NULL},
  {17, "Front Far East F8", 0, NULL},
  {18, "Jaleco SS8806", 0, NULL},
  {19, "Namcot 106", 0, NULL},
  {20, "Famicom Disk System", 0, NULL},
  {21, "Konami VRC4", 0, NULL},
  {22, "Konami VRC2 v1", 0, NULL},
  {23, "Konami VRC2 v2", 0, NULL},
  {24, "Konami VRC6", 0, NULL},
  {25, "Unknown", 0, NULL},
  {26, "Unknown", 0, NULL},
  {27, "Unknown", 0, NULL},
  {28, "Unknown", 0, NULL},
  {29, "Unknown", 0, NULL},
  {30, "Unknown", 0, NULL},
  {31, "Unknown", 0, NULL},
  {32, "Irem G-101", 0, NULL},
  {33, "Taito TC0190/TC0350", 0, NULL},
  {34, "iNES Mapper #34", 0, NULL}};


/* EOF */
