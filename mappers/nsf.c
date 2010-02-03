#include <stdlib.h>
#include <string.h>

int nsf_mapper_init (void)
{
   memset((void *)nes.ppu.vram, 0, 0x2000);
   return 1;
}

byte ram32k_read (register word Addr)
{
    return ram32k[Addr & 0x7FFF];
}

int nsf_save_state (chunk_writer_t writer, void *arg)
{
    return writer(arg, "NSF", ram32k, sizeof(ram32k));
}

int nsf_restore_state (chunk_reader_t reader, void *arg)
{
    return reader(arg, "NSF", ram32k, sizeof(ram32k));
}

static byte fake_exram[0x400];

void nsf_exram_write (register word Addr, register byte Value)
{
    Addr -= 0x5C00;
    //printf("NSF: wrote $%04X <- $%02X\n", Addr, Value);
    if (Addr < 0x3F8) fake_exram[Addr] = Value;
    if (Addr < 0x400) nsf_load_bank(Addr & 7, Value);
}

byte nsf_exram_read (register word Addr)
{
    Addr -= 0x5C00;
    if (Addr < 0x3F8) return fake_exram[Addr];
    else if (Addr < 0x400) {
        printf("Read from NSF bank register %X\n", Addr);
        return 0;
    }
    else return 0xFF;
}


struct mapper_methods mapper_NSF = {
   nsf_mapper_init,
   mapper0_shutdown,
   mapper_noprgwrite,
   ram32k_read,
   mapper_ignores_scanline_start,
   mapper_ignores_scanline_end,
   nsf_save_state,
   nsf_restore_state,
   nsf_exram_write,
   nsf_exram_read
};
