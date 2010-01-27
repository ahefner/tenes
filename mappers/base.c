#include <stdlib.h>
#include <string.h>

int mapper0_init(void)
{
    if (nes.rom.chr) memcpy((void *)nes.ppu.vram,(void *)nes.rom.chr,0x2000);
    else memset((void *)nes.ppu.vram, 0, 0x2000);
    printf("Mapper 0 init.\n");
    return 1;
}

void mapper0_shutdown(void)
{
    printf("Mapper 0 shutdown\n");
}

void mapper_noprgwrite(register word Addr,register byte Value)
{ 
}

byte mapper0_read(register word Addr)
{
    return nes.rom.prg[Addr & (nes.rom.prg_size-1)];
}

/*
void mapper_novramwrite(register word Addr,register byte Value)
{ }

byte mapper_vramread(register word Addr)
{   
   return nes.ppu.vram[Addr];
}

*/

void mapper_ignores_scanline_start (void)
{
    return;
}

int mapper_ignores_scanline_end (void)
{
    return 0;
}

int nop_save_state (chunk_writer_t writer, void *x)
{
    return 1;
}

int nop_restore_state (chunk_reader_t reader, void *x)
{
    return 1;
}

void ignore_write (register word Addr, register byte Value)
{
}

byte ignore_read (register word Addr)
{
    return 0xFF;
}

struct mapper_methods mapper_None = {
    mapper0_init,
    mapper0_shutdown,
    mapper_noprgwrite,
    mapper0_read,
    mapper_ignores_scanline_start,
    mapper_ignores_scanline_end,
    nop_save_state,
    nop_restore_state,
    ignore_write,
    ignore_read
};
