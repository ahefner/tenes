struct {
    unsigned selected_bank;
    unsigned last_bank;
} camerica;

int mapper_camerica_init (void)
{
    nes.rom.mirror_mode = MIRROR_ONESCREEN;
    camerica.last_bank = nes.rom.prg_size - 0x4000;
    camerica.selected_bank = 0;
    return 1;
}

void mapper_camerica_shutdown (void)
{
}

void mapper_camerica_write (register word addr, register byte value)
{
    int bank = value % (nes.rom.prg_size / 0x4000);

    if (!(addr & 0x4000)) printf("camerica: Curious write of %02X to %04X\n", value, addr);
    else {
        camerica.selected_bank = bank * 0x4000;
    }
}

byte mapper_camerica_read(register word addr)
{
    if (addr & 0x4000) return nes.rom.prg[camerica.last_bank + (addr & 0x3FFF)];
    else return nes.rom.prg[camerica.selected_bank + (addr & 0x3FFF)];
}

int camerica_save_state (chunk_writer_t writer, void *arg)
{
    return writer(arg, "Camerica driver v1", &camerica, sizeof(camerica));
}

int camerica_restore_state (chunk_reader_t reader, void *arg)
{
    return reader(arg, "Camerica driver v1", &camerica, sizeof(camerica));
}

struct mapper_methods mapper_camerica = 
{
    mapper_camerica_init,
    mapper_camerica_shutdown,
    mapper_camerica_write,
    mapper_camerica_read,
    mapper_ignores_scanline_start,
    mapper_ignores_scanline_end,
    camerica_save_state,
    camerica_restore_state
};

