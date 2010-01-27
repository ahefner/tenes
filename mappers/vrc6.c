
struct {
    int bank8;             /* 16 KB bank at 8000 */
    int bankC;             /*  8 KB bank at C000 */
    byte irq_latch;
    byte irq_count;
    byte irq_mode;
} vrc6;

int vrc6_init (void)
{
    printf("Konami VRC6\n");
    vrc6.bank8 = 0;
    vrc6.bankC = 0;
    vrc6.irq_latch = 0;
    vrc6.irq_count = 0;
    vrc6.irq_mode = 0;
    return 1;
}

void vrc6_write (register word addr, register byte value)
{
    switch (addr & 0xF000) {
    case 0x8000:                /* Program 16 KB bank */
        vrc6.bank8 = value & ((nes.rom.prg_size / 0x4000)-1);
        break;

    case 0xB000:                /* Mirroring control */
        switch ((value >> 2) & 3) {
        case 0: 
            nes.mirror_mode = MIRROR_VERT;
            break;
        case 1:
            nes.mirror_mode = MIRROR_HORIZ;
            break;
        case 2:
            nes.mirror_mode = MIRROR_ONESCREEN;
            nes.onescreen_page = 0;
            break;
        case 3:
            nes.mirror_mode = MIRROR_ONESCREEN;
            nes.onescreen_page = 0x400; /* Not verified.. */
            break;
        }
        break;

    case 0xC000:                /* Program 8 KB bank */
        vrc6.bankC = value & ((nes.rom.prg_size / 0x2000)-1);
        break;        
    case 0xD000:                /* Character bank select */
    case 0xE000:
        value &= ((nes.rom.chr_size / 0x400)-1);
        memcpy(nes.ppu.vram + ((addr & 0x1000) ^ 0x1000) + (0x400 * (addr & 3)),
               nes.rom.chr + 0x400 * value, 
               0x400);
        break;
    case 0xF000:
//        printf("VRC IRQ write: %4X <- %2X\n", addr, value);
        switch (addr & 3) {
        case 0:                 /* Load IRQ Latch */
            vrc6.irq_latch = value;
            break;
        case 1:                 /* Set IRQ Mode */
            vrc6.irq_mode = value;
            if (value & 2) vrc6.irq_count = vrc6.irq_latch;
            break;
        case 2:                 /* IRQ Acknowledge */
            if (vrc6.irq_mode & 1) vrc6.irq_mode &= 2;
            else vrc6.irq_mode &= ~2;
            break;
        }
    }
}

byte vrc6_read (register word addr)
{
    switch (addr & 0xE000) {
    case 0x8000:
    case 0xA000:
        return nes.rom.prg[vrc6.bank8 * 0x4000 + (addr & 0x3FFF)];
    case 0xC000:
        return nes.rom.prg[vrc6.bankC * 0x2000 + (addr & 0x1FFF)];
    case 0xE000:
        return nes.rom.prg[nes.rom.prg_size - 0x2000 + (addr & 0x1FFF)];
    default:
        return 0;        
    }
}

/* This is a quick kludge to implement the scanline mode of the VRC6
 * IRQ counter. It isn't accurate, but I might get away with it. */
int vrc6_scanline_end (void)
{
    if (vrc6.irq_mode & 2) {
        vrc6.irq_count--;
        if (!vrc6.irq_count) return 1;
    }

    return 0;
}

struct mapper_methods mapper_vrc6 = {
   vrc6_init,
   mapper0_shutdown,
   vrc6_write,
   vrc6_read,
   mapper_ignores_scanline_start,
   vrc6_scanline_end,
   nop_save_state,
   nop_restore_state  
};
