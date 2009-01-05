
/* mapper 2 - Konami */

int mapper2_page;

int mapper2_init (void)
{
   mapper2_page = 0;
   return 1;
}

void mapper2_shutdown (void)
{
}

void mapper2_write (register word Addr, register byte Value)
{
    unsigned num_pages = nes.rom.prg_size / 0x4000;
    if (Value >= num_pages) Value %= num_pages;
    mapper2_page = Value;
}

byte mapper2_read (register word Addr)
{
    if (Addr >= 0xC000) return nes.rom.prg[(Addr & 0x3FFF) + nes.rom.prg_size - 0x4000];
    else return nes.rom.prg[mapper2_page * 0x4000 + (Addr & 0x3FFF)];
    
}

int mapper2_save_state (FILE *out)
{
    return write_state_chunk(out, "Mapper 2 driver v1", &mapper2_page, sizeof(mapper2_page));
}

int mapper2_restore_state (FILE *in)
{
    return read_state_chunk(in, "Mapper 2 driver v1", &mapper2_page, sizeof(mapper2_page));
}

struct mapper_methods mapper_konami = {
   mapper2_init,
   mapper2_shutdown,
   mapper2_write,
   mapper2_read,
   mapper0_scanline,
   mapper2_save_state, 
   mapper2_restore_state
};
   
