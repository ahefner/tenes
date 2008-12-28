#ifndef MAPPER_INFO_H
#define MAPPER_INFO_H

/* NES mappers */

#include "M6502/M6502.h"

struct mapper_methods
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
   struct mapper_methods *methods;
};
   

/* MapTable - contains mapper id/support level
   Entry in maptable should match iNes Mapper # (id) */

#ifndef MAPPER_INFO_C
extern struct mapperinfo *mapper_find (int ines_number);
extern struct mapper_methods mapper_None;
#endif

#endif
