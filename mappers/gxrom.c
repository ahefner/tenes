
byte mapper66_val = 0;

int mapper66_init (void)
{
    mapper66_val = 0xFF;
    return 1;
}

void mapper66_write (register word Addr,register byte Value)
{
    const byte prg_page = (Value & 0x30) >> 4;
    const byte chr_page = (Value & 0x03);

    mapper_select_chr_page(0,chr_page*2);
    mapper_select_chr_page(1,chr_page*2+1);

    mapper66_val = prg_page;
}

byte mapper66_read (register word addr)
{
    unsigned offset = ((addr & 0x7FFF) + (mapper66_val * 0x8000)) & (nes.rom.prg_size-1);
    return nes.rom.prg[offset];
}

int mapper66_save_state (chunk_writer_t writer, void *arg)
{
    return writer(arg, "GxROM", &mapper66_val, sizeof(mapper66_val));
}

int mapper66_restore_state (chunk_reader_t reader, void *arg)
{
    return reader(arg, "GxROM", &mapper66_val, sizeof(mapper66_val));
}

struct mapper_methods mapper_GxROM = {
    mapper66_init,
    mapper0_shutdown,
    mapper66_write,
    mapper66_read,
    mapper_ignores_scanline_start,
    mapper_ignores_scanline_end,
    mapper66_save_state,
    mapper66_restore_state,
    ignore_write,
    ignore_read
};
