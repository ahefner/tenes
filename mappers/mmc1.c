/* MMC1 mapper */

int mmc1_prg_pages; /* 16k */
int mmc1_chr_pages; /* 4k */
int mmc1_reg[4];
byte* mmc1_last_page_ptr;
int mmc1_large_bank;
int mmc1_large_bank_mask;


#define REG0_MIRROR_MODE        BIT(0)
#define REG0_ONE_SCREEN_ENABLE  BIT(1)
#define REG0_SWITCH_HIGH_LOW    BIT(2)
#define REG0_SIZE_32K_16K       BIT(3)
#define REG0_CHROM_8KB_4KB      BIT(4)

#define BANK mmc1_reg[3]



//int MMC1_bank32flag; 

/* TODO: Support for 512k and 1024k roms */

int mmc1_init(void)
{
    mmc1_prg_pages=nes.rom.prg_size/0x4000;
    mmc1_chr_pages=nes.rom.chr_size/0x1000;

    if (mmc1_chr_pages) memcpy((void *)nes.ppu.vram,(void *)nes.rom.chr,0x2000);

    mmc1_reg[0] = 0x0C;  /* we boot in 16KB switching LOW bank, and 4KB CHROM pages */
    mmc1_reg[1] = 0;
    mmc1_reg[2] = 0;
    mmc1_reg[3] = 0;

    BANK = 0;
    mmc1_last_page_ptr = nes.rom.prg + ((mmc1_prg_pages-1) & 15)*KB(16);

    mmc1_large_bank=0;
    mmc1_large_bank_mask=0;
    if (nes.rom.prg_size==KB(512)) mmc1_large_bank_mask=1;
    else if (nes.rom.prg_size==KB(1024)) mmc1_large_bank_mask=1; /* Fiction? */

    mmc1_print_status();

    return 1;
}

void mmc1_shutdown (void)
{
    free(nes.mapper_data);
}

void mmc1_print_status (void)
{
    printf("MMC1: %s %s %s %s\n",
           mmc1_reg[0] & REG0_SWITCH_HIGH_LOW ? "Switching LOW" : "Switching HIGH",
           mmc1_reg[0] & REG0_SIZE_32K_16K ? "16k" : "32k",
           mmc1_reg[0] & REG0_MIRROR_MODE ? "Vertical" : "Horizontal",
           nes.rom.chr_size? 
           (mmc1_reg[0] & REG0_CHROM_8KB_4KB ? "CHR:4KB" : "CHR:8KB") : 
           (mmc1_reg[0] & BIT(4) ? "Large mode 0" : "Large mode 1"));
}

void mmc1_short_status (void)
{
    printf("(r0..3=%02X,%02X,%02X,%02X large=%i,mask=%i)", 
           mmc1_reg[0], mmc1_reg[1], mmc1_reg[2], mmc1_reg[3], 
           mmc1_large_bank, mmc1_large_bank_mask);
}

void mmc1_write_reg (unsigned reg, unsigned val)
{
    int pagebase;
    assert(reg<4);

    //printf("mmc1_write: %02X to register %i\n", val, reg);
    
    switch (reg)
    {
    case 0:
        mmc1_reg[0]=val;
//        if (nes.cpu.Trace)
        mmc1_print_status();
        nes.rom.mirror_mode=(!(val & REG0_ONE_SCREEN_ENABLE)) ?
            MIRROR_ONESCREEN : ((val & REG0_MIRROR_MODE) ? MIRROR_HORIZ : MIRROR_VERT);
        break;
    case 1:
        mmc1_reg[1]=val;
        pagebase = val & 0x1F;

        mmc1_large_bank = (val >> 4) & 1;

        if (trace_ppu_writes)
            printf("MMC1: switch low CHR: pagebase = %i, pages = %i, switching both? %s\n",
                   pagebase, mmc1_chr_pages, mmc1_reg[0] & REG0_CHROM_8KB_4KB ? "No" : "Yes"); 
        if (pagebase < mmc1_chr_pages) {          
            if (!(mmc1_reg[0] & REG0_CHROM_8KB_4KB) && (pagebase < mmc1_chr_pages-1))
                memcpy((void *)nes.ppu.vram,(void *)nes.rom.chr + 0x1000 * pagebase,0x2000);
            else memcpy((void *)nes.ppu.vram,(void *)nes.rom.chr + 0x1000 * pagebase,0x1000);
            vid_tilecache_dirty = 1;
        }
        break;
    case 2:
        mmc1_reg[2]=val;
        pagebase = val & 0x1F;
        if ((pagebase < mmc1_chr_pages) && 
            (mmc1_reg[0] & REG0_CHROM_8KB_4KB)) {
            memcpy((void *)nes.ppu.vram + 0x1000,
                   (void *)nes.rom.chr + 0x1000 * pagebase,
                   0x1000);
            vid_tilecache_dirty = 1;
        }
        break;
    case 3:
        // Wild guess.
        if (!(mmc1_reg[0] & REG0_SIZE_32K_16K)) val &= ~1;
        //if (!(mmc1_reg[0] & REG0_SIZE_32K_16K)) printf("**** 32k mode *****\n");
        if (nes.cpu.Trace)
            printf("MMC1: -> bank req %i so %i of %i\n", val, val % mmc1_prg_pages, mmc1_prg_pages);
        mmc1_reg[3] = val % mmc1_prg_pages;
        break;
    }
}

void mmc1_write (register word addr, register byte value)
{
    static unsigned accum=0, counter=0;
    unsigned n = (addr & 0x6000) >> 13;
    int *reg = mmc1_reg;

    if (GETBIT(7,value)) /* mmc1 reset */
    {
        byte reset = (reg[0] & 0x10) | 12;
        /* Are the other registers reset here? Does it matter? */
        //printf("mmc1: reset\n");
        reg[0] = reset;
        accum = 0;
        counter = 0;
      
    }
    else
    {
        accum=(accum>>1)+(GETBIT(0,value)<<4);
        /* accum=accum | (GETBIT(0,value)<<counter); */
        counter++;
        //printf("mmc1: wrote bit. accum=%02X, counter=%i\n", accum, counter);
        if (counter==5)
        {
            /* printf("$%02X -> register %i ($%04X)\n",accum,n,addr); */
            mmc1_write_reg(n,accum);
            counter=accum=0;
        }
    }
}

byte mmc1_read (register word addr)
{
    word offset = addr&0x3FFF; /* offset within a 16kb page */
    byte *prg = nes.rom.prg;
    int *reg = mmc1_reg;
    unsigned large_offset = 0;

    large_offset = mmc1_large_bank;
    large_offset &= mmc1_large_bank_mask;
    large_offset *= KB(256);
  
    if (reg[0] & REG0_SIZE_32K_16K)
    { /* 16KB switching mode */
        if (GETBIT(14,addr)^GETBIT(2,reg[0])) 
        { /* Read from the switching page */
            //printf("  read %04X -- bank %i\n", addr, BANK);
            return prg[BANK*KB(16)+large_offset+offset];
        }
        else
        { /* Read from hardwired page */
            if (reg[0] & REG0_SWITCH_HIGH_LOW)
            { /* Hardwired 0xC000 bank */
                //printf("  read $%04X (last page, hardwired at $C000)\n", addr);
                return mmc1_last_page_ptr[offset + large_offset];
            }
            else
            { /* Hardwired 0x8000 bank */
                //printf("  read $%04X (first page, hardwired at $8000)\n", addr);
                return prg[offset + large_offset];
            }
        }
    }
    else
    { /* 32KB switching mode */
        return prg[KB(32)*(BANK>>1) + large_offset + (addr & 0x7FFF)];
    }
}

struct mapper_methods mapper_MMC1 = {
    mmc1_init,
    mmc1_shutdown,
    mmc1_write,
    mmc1_read,
    mapper0_scanline
};
