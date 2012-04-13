/* Nintendo MMC3 */

struct {
    int chrpages; /* # of 1k VROM pages */
    int prgpages; /* # of 8k PRG pages */
    byte reg8000;

    /* The MMC3 switches two 8 KB banks. Depending on bit 6 of register
       $8000, these banks are either $8000/$A000 or $A000/$C000. The other
       two banks will be mapped to the last two pages of the cart.
    */

    byte countdown, latched, latch_trigger;
    int irq_enabled;

    unsigned bank[2];
} mmc3;

static inline byte *mmc3_getprgpage (int n)
{
    n &= (mmc3.prgpages - 1);
    return nes.rom.prg + 0x2000*n;
}

void mmc3_select_chr (int dest, int n)
{
    if (mmc3.chrpages) {
        while (n >= mmc3.chrpages) n -= mmc3.chrpages;
        memcpy (nes.ppu.vram + dest*0x400, nes.rom.chr + 0x400 * n, 0x400);
    }
}

int mmc3_init (void)
{
    mmc3.chrpages = nes.rom.chr_size / 0x400;
    mmc3.prgpages = nes.rom.prg_size / 0x2000;

    mmc3_select_chr(0,0);
    mmc3_select_chr(1,1);
    mmc3_select_chr(2,2);
    mmc3_select_chr(3,3);

    mmc3.reg8000 = 0x00;
    mmc3.bank[0] = 0;
    mmc3.bank[1] = 1;
    mmc3.countdown = 0;
    mmc3.latched = 0;
    mmc3.latch_trigger = 0;
    mmc3.irq_enabled = 0;

    printf ("MMC3 initialized. %i CHR pages (%ik), %i PRG pages (%ik).\n",
            mmc3.chrpages, nes.rom.chr_size/1024, mmc3.prgpages, nes.rom.prg_size/1024);
    return 1;
}

void mmc3_shutdown (void)
{
}

byte mmc3_read (register word addr)
{
    unsigned page = (addr&0x6000) >> 13;
    int S = (mmc3.reg8000 & BIT(6)) >> 6;
    unsigned adjusted = ((page - S) & 3) ^ S;
    if (adjusted < 2) return mmc3_getprgpage(mmc3.bank[adjusted])[addr&0x1FFF];
    else return mmc3_getprgpage(mmc3.prgpages - 2 + (page&1))[addr&0x1FFF];
}

void mmc3_write (register word addr, register byte value)
{
    int cmd = mmc3.reg8000 & 7;
    addr &= 0xE001;

    switch (addr) {

    case 0x8000:
        mmc3.reg8000 = value;
        break;

    case 0x8001:
        if (cmd < 6) {
            int dest, num = 1, i;
            /* VROM switch */
            switch (cmd) {
            case 0:  /* FIXME: Command 0 ignores LSB of page# */
                dest = 0;
                num = 2;
                break;
            case 1: /* FIXME: Command 1 always lets LSB of page# equal 1 */
                dest = 2;
                num = 2;
                break;
            default:
                dest = (cmd - 2) + 4;
                break;
            }
            if (trace_ppu_writes)
                printf ("%sMMC3: switching %s VROM. page=%i, chose %i (%i banks at %i)\n", nes_time_string(), mmc3.reg8000 & 0x80 ? "inverted" : "normal", cmd, value, num, dest);
            if (mmc3.reg8000 & 0x80) dest = dest ^ 4;
            for (i=0; i<num; i++) mmc3_select_chr (dest+i, value+i);

        } else {   /* PRG-ROM switch */
            int bank = mmc3.reg8000 & 1; /* LSB of 6 or 7 */
            if (nes.cpu.Trace)
                printf ("%sMMC3: Switching program - bank %u (mode %i) to page %u.\n",
                        nes_time_string(), (unsigned)bank, ((unsigned)mmc3.reg8000 & 0x40)>>6,
                        (unsigned)value);

            mmc3.bank[bank] = value;
        }
        break;

    case 0xA000:
        nes.mirror_mode = value&1? MIRROR_HORIZ : MIRROR_VERT;
        break;

    case 0xA001:
        //printf ("MMC3: Saveram toggle?\n");
        break;

    case 0xC000:
        if (trace_ppu_writes)
            printf ("%sMMC3: IRQ countdown temporary = %i\n", nes_time_string(), (int)value);
        mmc3.latched = value;
        break;

    case 0xC001:
        if (trace_ppu_writes)
            printf ("%sMMC3: IRQ latch request (wrote %i)\n", nes_time_string(), (int)value);
        //mmc3.latch_trigger = 1;
        mmc3.countdown = 0;
        break;

    case 0xE000:
        if (trace_ppu_writes) printf("MMC3: IRQ CR0: IRQ disabled.\n");
        mmc3.irq_enabled = 0;
        break;

    case 0xE001:
        if (trace_ppu_writes)
            printf ("%u.%u: MMC3: IRQ CR1: IRQ enabled.\n", nes.time, nes.scanline);
        mmc3.irq_enabled = 1;
        break;

    default:
        printf("MMC3: wrote %02X to %X ???\n", value, addr);
    }
}


void mmc3_reload_irq_counter (void)
{
    const int latched_offset = 0;
    /* The timing here is a real pain in the ass. A value of zero
     * fixes the scrolling minigame in SMB3 and the water level in
     * TMNT3, but introduces a glitch in TMNT2. */
    mmc3.countdown = mmc3.latched + latched_offset;
    //if (mmc3.latched == 0) printf("%s: MMC3 latched zero! Interesting!\n", nes_time_string());
}

/* The scanline counter would be a real pain to emulate precisely, but we can more or less fake it. */

void mmc3_scanline_start (void)
{

}

/* A real MMC3 chip counts scanlines by some clever monitoring of the
 * PPU address lines. We fake it by providing per-scanline hooks into
 * the mapper. */
int mmc3_scanline_end (void)
{
    // printf("%u.%u: mmc3 scanline registered. %u %u %u\n", nes.time, nes.scanline, mmc3.countdown, mmc3.latched, mmc3.latch_trigger);

    /* If this interpretation proves to be right, we can eliminate the
       latch trigger and simply zero the counter.  But I don't think
       that would be right.
    */
    if (0) //trace_ppu_writes)
        printf("mmc3_scanline: countdown %i latched %i trigger %i irq_enabled %i\n", mmc3.countdown, mmc3.latched, mmc3.latch_trigger, mmc3.irq_enabled);

/*    if (mmc3.latch_trigger) {
        mmc3_reload_irq_counter();
        mmc3.latch_trigger = 0;
        }*/

    if (mmc3.countdown == 0) {
        mmc3_reload_irq_counter();
    } else if (mmc3.countdown) {
        mmc3.countdown--;
        if ((mmc3.countdown == 0) && mmc3.irq_enabled) {
            if (trace_ppu_writes) printf("%sMMC3 IRQ\n", nes_time_string());
            return 1;
        }
    }
    return 0;
}

int mmc3_save_state (chunk_writer_t writer, void *arg)
{
    return writer(arg, "MMC3 driver v1", &mmc3, sizeof(mmc3));
}

int mmc3_restore_state (chunk_reader_t reader, void *arg)
{
    return reader(arg, "MMC3 driver v1", &mmc3, sizeof(mmc3));
}

struct mapper_methods mapper_MMC3 = {
    mmc3_init,
    mmc3_shutdown,
    mmc3_write,
    mmc3_read,
    mmc3_scanline_start,
    mmc3_scanline_end,
    mmc3_save_state,
    mmc3_restore_state,
    ignore_write,
    ignore_read
};
