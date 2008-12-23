/* Nintendo MMC3 mapper */
/* this should be interesting... */

int mmc3_init (void);
void mmc3_shutdown (void);
void mmc3_write (register word addr, register byte value);
byte mmc3_read (register word addr);
int mmc3_scanline (void);

struct mapper_functions mapper_MMC3 = {
    mmc3_init,
    mmc3_shutdown,
    mmc3_write,
    mmc3_read,
    mmc3_scanline
};

int mmc3_chrpages; /* # of 1k VROM pages */
int mmc3_prgpages; /* # of 8k PRG pages */

byte mmc3_8000;

/* The MMC3 switches two 8 KB banks. Depending on bit 6 of register
   $8000, these banks are either $8000/$A000 or $A000/$C000. The other
   two banks will be mapped to the last two pages of the cart.
*/
byte *mmc3_ram[2];

byte mmc3_countdown = 0, mmc3_latched = 0, mmc3_latch_trigger = 0;
int mmc3_irq_enabled = 0;

byte *mmc3_getprgpage (int n)
{
    n &= (mmc3_prgpages - 1);
    return nes.rom.prg + 0x2000*n;
}

void mmc3_select_chr (int dest, int n)
{
    if (mmc3_chrpages) {
        while (n >= mmc3_chrpages) n -= mmc3_chrpages;
        memcpy (nes.ppu.vram + dest*0x400, nes.rom.chr + 0x400 * n, 0x400);
    }
}

int mmc3_init (void)
{
    mmc3_chrpages = nes.rom.chr_size / 0x400;
    mmc3_prgpages = nes.rom.prg_size / 0x2000;
  
    mmc3_select_chr (0,0);
    mmc3_select_chr (1,1);
    mmc3_select_chr (2,2);
    mmc3_select_chr (3,3);

    mmc3_8000 = 0x00;
    mmc3_ram[0] = mmc3_getprgpage(0);
    mmc3_ram[1] = mmc3_getprgpage(1);
  
    printf ("MMC3 initialized. %i CHR pages (%ik), %i PRG pages (%ik).\n", 
            mmc3_chrpages, nes.rom.chr_size/1024, mmc3_prgpages, nes.rom.prg_size/1024);
    return 1;
}

void mmc3_shutdown (void)
{
}

byte mmc3_read (register word addr)
{
    unsigned page = (addr&0x6000) >> 13;
    int S = (mmc3_8000 & BIT(6)) >> 6;
    unsigned adjusted = ((page - S) & 3) ^ S;
    if (adjusted < 2) return mmc3_ram[adjusted][addr&0x1FFF];
    else return mmc3_getprgpage(mmc3_prgpages - 2 + (page&1))[addr&0x1FFF];
}

void mmc3_write (register word addr, register byte value)
{
    int cmd = mmc3_8000 & 7;
    addr &= 0xE001;

    switch (addr) {

    case 0x8000:
        mmc3_8000 = value;
        break;

    case 0x8001:
        if (cmd < 6) {
            int dest, num = 1, i;
            /* VROM switch */
            // printf ("%u: MMC3: switching %s VROM. page=%i, chose %i\n", frame_number, mmc3_8000 & 0x80 ? "inverted" : "normal", cmd, value);
            switch (cmd) {
            case 0:
                dest = 0;
                num = 2;
                break;
            case 1:
                dest = 2;
                num = 2;
                break;
            default:
                dest = (cmd - 2) + 4;
                break;
            }
            if (mmc3_8000 & 0x80) dest = dest ^ 4;
            for (i=0; i<num; i++) mmc3_select_chr (dest+i, value+i);

        } else {   /* PRG-ROM switch */
            int bank = mmc3_8000 & 1; /* LSB of 6 or 7 */
            if (0)
                printf ("%u: MMC3: Switching program - bank %u (mode %i) to page %u.\n", 
                        frame_number, (unsigned)bank, ((unsigned)mmc3_8000 & 0x40)>>6, 
                        (unsigned)value);
               
            mmc3_ram[bank] = mmc3_getprgpage(value);
        }
        break;

    case 0xA000:
        //printf("MMC3: mirroring select: %i\n", value&1);
        nes.rom.mirror_mode = value&1? MIRROR_HORIZ : MIRROR_VERT;
        break;

    case 0xA001:
        //printf ("MMC3: Saveram toggle.\n");
        break;

    case 0xC000:
        //printf ("%u.%u: MMC3: IRQ countdown register latched %i\n", frame_number, nes.scanline, (int)value);
        mmc3_latched = value;
        break;

    case 0xC001:
        //printf ("%u.%u: MMC3: IRQ latch trigger (wrote %i)\n", frame_number, nes.scanline, (int)value);
        mmc3_latch_trigger = 1;
        break;

    case 0xE000:
        //printf ("MMC3: IRQ CR0: IRQ disabled.\n");
        mmc3_irq_enabled = 0;
        break;

    case 0xE001:
        //printf ("%u.%u: MMC3: IRQ CR1: IRQ enabled.\n", frame_number, nes.scanline);
        mmc3_irq_enabled = 1;
        break;
        
    default:
        printf("MMC3: wrote %02X to %X ???\n", value, addr);
    }      
}

/* This really counts by some clever monitoring of the PPU address
 * lines. We fake it by providing a per-scanline hook into the
 * mapper. */
int mmc3_scanline (void)
{
    // printf("%u.%u: mmc3 scanline registered. %u %u %u\n", frame_number, nes.scanline, mmc3_countdown, mmc3_latched, mmc3_latch_trigger);

    if (mmc3_latch_trigger) {
        mmc3_countdown = mmc3_latched + 1;
        mmc3_latch_trigger = 0;
    }

    if (mmc3_countdown) {
        mmc3_countdown--;
        if (!mmc3_countdown && mmc3_irq_enabled) {
            //printf("%u.%u: MMC3 IRQ\n", frame_number, nes.scanline);
            return 1;
        }
    }
    return 0;
}

