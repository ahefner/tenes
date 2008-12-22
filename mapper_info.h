#ifndef MAPPER_INFO_H
#define MAPPER_INFO_H

/* NES mappers */

#include "M6502/M6502.h"

/* iNES Mapper types */
#define MAPPER_NONE     0
#define MAPPER_MMC1     1
#define MAPPER_KONAMI   2
#define MAPPER_VROMS    3
#define MAPPER_MMC3     4
#define MAPPER_MMC5     5
#define MAPPER_FFE_F4   6
#define MAPPER_ROMS     7
#define MAPPER_FFE_F3   8
#define MAPPER_MMC2     9
#define MAPPER_MMC4     10
#define MAPPER_COLORD   11
#define MAPPER_FFE_F6   12
#define MAPPER_100      15
#define MAPPER_BANDAI   16
#define MAPPER_FFE_F8   17
#define MAPPER_JALECO   18
#define MAPPER_NAMCOT   19
#define MAPPER_FAMDISK  20
#define MAPPER_INES34   34
/* damn this is boring */


#define	MAPTABLESIZE		35
#define	MapTableMax	MAPTABLESIZE-1

struct mapper_functions
{
   int (*mapper_init)(void); /* called during rom load after CHR and PPU data is loaded */
   void (*mapper_shutdown)(void);
   void (*mapper_write)(register word Addr,register byte Value);
   byte (*mapper_read)(register word Addr);
   int (*scanline)(void);
/*
   void (*vram_write)(register word Addr,register byte Value);
   byte (*vram_read)(register word Addr);
 */
};

struct mapperinfo 
{    
   int id;	       
   char *name; 	       
   int support;	     
   struct mapper_functions *info;   
};
   

/* MapTable - contains mapper id/support level
   Entry in maptable should match iNes Mapper # (id) */

#ifndef MAPPER_INFO_C
extern struct mapperinfo MapTable[MAPTABLESIZE];
#endif

#endif
