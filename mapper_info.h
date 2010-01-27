#ifndef MAPPER_INFO_H
#define MAPPER_INFO_H

#include <stdio.h>
#include "global.h"

/* NES mappers */

typedef int (*chunk_writer_t) (void *private, char *name, void *data, unsigned length);
typedef int (*chunk_reader_t) (void *private, char *name, void *data, unsigned length);

struct mapper_methods
{
    int (*mapper_init)(void); /* called during rom load after CHR and PPU data is loaded */
    void (*mapper_shutdown)(void);
    void (*mapper_write)(register word Addr, register byte Value);
    byte (*mapper_read)(register word Addr);
    void (*scanline_start)(void);
    int (*scanline_end)(void);
    int (*save_state) (chunk_writer_t writer, void *arg);
    int (*restore_state) (chunk_reader_t reader, void *arg);
    void (*ex_write)(register word Addr, register byte Value);
    byte (*ex_read)(register word Addr);
/* void (*vram_write)(register word Addr,register byte Value);
   byte (*vram_read)(register word Addr); */
};

struct mapperinfo 
{    
   int id;	       
   char *name; 	       
   struct mapper_methods *methods;
};
   
#ifndef MAPPER_INFO_C
extern struct mapperinfo *mapper_find (int ines_number);
extern struct mapper_methods mapper_None;
struct mapperinfo *get_NSF_minf (void);
#endif

#endif
