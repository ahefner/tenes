/* mappers */

#include <stdlib.h>
#include <string.h>


int mapper0_init(void)
{
   nes.mapper_data=malloc(0x8000);
   switch(nes.rom.prg_size)
     {
      case 0x8000:
	memcpy(nes.mapper_data,(void *)nes.rom.prg,0x8000);
	break;
      case 0x4000:
	memcpy(nes.mapper_data,(void *)nes.rom.prg,0x4000);
	memcpy((((byte *)nes.mapper_data)+0x4000),(void *)nes.rom.prg,0x4000);
	break;
      default:
	memcpy(nes.mapper_data,(void *)nes.rom.prg,0x8000);
	printf("warning: Unknown mapper0 configuration.\n");
	break;	
     }
   memcpy((void *)nes.ppu.vram,(void *)nes.rom.chr,0x2000);
   printf("Mapper 0 init.\n");
   return 1;
}

void mapper0_shutdown(void)
{
   printf("Mapper 0 shutdown\n");
   free(nes.mapper_data);
}

void mapper_noprgwrite(register word Addr,register byte Value)
{ }

byte mapper0_read(register word Addr)
{
   return ((byte *)nes.mapper_data)[Addr&0x7FFF];
}

/*
void mapper_novramwrite(register word Addr,register byte Value)
{ }

byte mapper_vramread(register word Addr)
{   
   return nes.ppu.vram[Addr];
}

*/

int mapper0_scanline (void)
{
    return 0;
}

struct mapper_functions mapper_None = {
   mapper0_init,
   mapper0_shutdown,
   mapper_noprgwrite,
   mapper0_read,
   mapper0_scanline
};
