byte *camerica_selected_bank;
byte *camerica_last_bank;

int mapper_camerica_init (void)
{
    nes.rom.mirror_mode = MIRROR_ONESCREEN;
    camerica_last_bank = nes.rom.prg + nes.rom.prg_size - 0x4000;
    camerica_selected_bank = nes.rom.prg;
    return 1;
}

void mapper_camerica_shutdown (void)
{
}

void mapper_camerica_write (register word addr,register byte value)
{
    int bank = value % (nes.rom.prg_size / 0x4000);

    if (!(addr & 0x4000)) printf("camerica: Curious write of %02X to %04X\n", value, addr);
    else {
        //printf("camerica: wrote %02X. switching to %i.\n", (int)value, bank);
        camerica_selected_bank = nes.rom.prg + bank * 0x4000;
    }
}

byte mapper_camerica_read(register word addr)
{
    if (addr & 0x4000) return camerica_last_bank[addr & 0x3FFF];
    else return camerica_selected_bank[addr & 0x3FFF];
}

struct mapper_methods mapper_camerica = 
{
    mapper_camerica_init,
    mapper_camerica_shutdown,
    mapper_camerica_write,
    mapper_camerica_read,
    mapper0_scanline
};

