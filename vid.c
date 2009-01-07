
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "sys.h"
#include "nes.h"
#include "global.h"

/* TODO: The tile cache is at this point probably a pessimization on
 * all but the simplest games, and I'd be better off eliminating the
 * last use of it.*/

int vid_tilecache_dirty = 1;

void render_clear (void)
{
    tv_scanline = 0;
    // I swear I intended to do something else here, but I forget what.
}

/* stupid tile cache */

typedef unsigned char tile[64];
typedef tile tile_group[9];
typedef tile_group tc_page[256];

tc_page tilecache[2];

static inline void unpack_chr_byte (byte line[8], unsigned char *chrdata)
{
    byte low = chrdata[0], high = chrdata[8];
    line[7] = ((low &   1) >> 0) | ((high & 1)   << 1);
    line[6] = ((low &   2) >> 1) | ((high & 2)   >> 0);
    line[5] = ((low &   4) >> 2) | ((high & 4)   >> 1);
    line[4] = ((low &   8) >> 3) | ((high & 8)   >> 2);
    line[3] = ((low &  16) >> 4) | ((high & 16)  >> 3);
    line[2] = ((low &  32) >> 5) | ((high & 32)  >> 4);
    line[1] = ((low &  64) >> 6) | ((high & 64)  >> 5);
    line[0] = ((low & 128) >> 7) | ((high & 128) >> 6);
}

void render_tile (byte *chrdata, tile image, unsigned char cols[4])
{
    unsigned char l, i;
    for (l = 0; l < 8; l++) {
        unpack_chr_byte(&image[l*8], chrdata + l);
        for (i=0; i<8; i++) image[l*8+i] = cols[image[l*8+i]];
    }
}

static inline void unpack_tile (byte out[8], int page, byte tileindex, byte y_offset)
{
    word addr = page * 0x1000 + tileindex * 16 + y_offset;
    unpack_chr_byte(out, nes.ppu.vram + addr);
}

/* builds a cache group for one character */
void gen_group (unsigned char *chrdata, tile_group grp)
{ 
  unsigned char cols[4];
  int i, j;

  cols[0] = 0;
  cols[1] = 1;
  cols[2] = 2;
  cols[3] = 3;
  render_tile (chrdata, grp[8], cols);

  for (i = 0; i < 8; i++) {
    for (j = 1; j < 4; j++) cols[j] = nes.ppu.vram[0x3F00 + i * 4 + j];     
    cols[0] = nes.ppu.vram[0x3F00];
    for (j=0; j<64; j++) {
      grp[i][j] = cols[grp[8][j]];
      if (grp[8][j]) grp[i][j]|=64;
    }
  }
}

/* builds a tile cache page for a full pattern table */
void gen_page (unsigned char *patterns, int pageidx)
{
  int i;

  for (i = 0; i < 256; i++) {
    gen_group (patterns + i * 16, tilecache[pageidx][i]);
  }
}

static const byte bit_reverse[256] =
{
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

/* The  2C02  can  simultaneously  render  up  to  eight  sprites  per
 * scanline. An initial pass  (performed in parallel with the previous
 * scanline on  the real hardware) examines the  64-entry sprite table
 * to locate the  first 8 sprites visible on the  coming line. I model
 * the rendering  process using  8 sprite_unit structures,  clocked in
 * parallel for  each pixel, which  each output a 4-bit  palette index
 * and priority bit. Sprite units  consist of a counter initialized to
 * the  sprite's  X  coordinate  which  decrements  each  pixel  while
 * nonzero. When the counter is  zero, the sprite output is built from
 * the priority/attribute  bits of  sprite flags and  the LSBs  of the
 * low/high  (left)  shift  registers,  and the  shift  registers  are
 * clocked.
 */
struct sprite_unit {
    byte low, high, flags;
    int number, counter;
};

/* Initializes the sprite units as described above. Returns the number
 * of sprite units containing sprites. This isn't strictly necessary,
 * as an unused sprite unit will output transparent pixels when
 * clocked, but knowing it can perhaps optimize rendering by not
 * clocking idle units. */
int sprite_evaluation (struct sprite_unit sprites[8], int scanline)
{
    int i, n = 0;
    int height = (nes.ppu.control1 & 0x20) ? 16 : 8;
    
    memset(sprites, 0, 8 * sizeof(struct sprite_unit));

    for (i=0; i<64; i++) {
        byte *spr = nes.ppu.spriteram + i*4;
        int y0 = spr[0];
        int sy = scanline - y0;
        byte *chrpage;

        if ((sy >= 0) && (sy < height)) {
            int num;
            if (spr[2] & 0x80) sy = height - sy - 1; /* Vertical flip */

            if (nes.ppu.control1 & 0x20) { /* 8x16 */
                num = (spr[1] & 0xFE) | (sy>=8 ? 1 : 0);
                chrpage = nes.ppu.vram + 0x1000 * (spr[1] & 1);
            } else { /* 8x8 */
                num = spr[1];
                chrpage = nes.ppu.vram + 0x1000 * ((nes.ppu.control1 >> 3) & 1);
            }

            sprites[n].flags = spr[2];
            sprites[n].number = i;
            sprites[n].low = chrpage[(sy&7) + num*16];
            sprites[n].high = chrpage[(sy&7) + num*16 + 8];
            sprites[n].counter = spr[3];

            if (!(spr[2] & 0x40)) { /* Horizontal flip */
                sprites[n].low = bit_reverse[sprites[n].low];
                sprites[n].high = bit_reverse[sprites[n].high];
            }

            n++;
        }

        /* We could emulate the sprite overflow flag here rather than
         * breaking, but the NES PPU itself seems to be buggy with
         * respect to this, and so far it hasn't seemed important. */
        if (n == 8) break;
    }

    return n;
}

/* Sprite unit output: 
   Bits 0,1 come from pattern table
   Bits 2,3 are the sprite attribute
   Bit 5 is the sprite priority (1 = Background, 0 = Foreground)
*/
static inline word sprite_unit_clock (struct sprite_unit *s)
{
    if (s->counter) {
        s->counter--;
        return 0;
    } else {
        byte output = (s->low & 1) | ((s->high & 1) << 1) | ((s->flags & 3) << 2) | ((s->flags & 0x20) >> 1);
        s->low >>= 1;
        s->high >>= 1;
        return output;
    }
}

void scanline_render_sprites (byte *dest)
{
    unsigned x, i, num_sprites;
    struct sprite_unit sprites[8];
    
    num_sprites = sprite_evaluation(sprites, tv_scanline);

    for (x=0; x<256; x++) {
        byte background = dest[x];
        byte sprite_output = 0;
        byte combined_output = background;

        sprite_output = sprite_unit_clock(&sprites[0]);
        /* Check for sprite 0 hit. Background pixels are already
         * palette-translated, but we're sneaky and mirrored the
         * palette so we can encode foreground versus background pixel
         * in bit 6, allowing rendering of sprites in a separate pass. */
        /* Supposedly we should ignore column 255? */
        if ((background & 0x40) && (!nes.sprite0_detected) && ((x >= 8) || ((nes.ppu.control2 & 4))) &&
            (sprite_output & 3) && (x < 255) && !sprites[0].number) {
            //nes.ppu.hit_flag = 1;
            nes.sprite0_hit_cycle = nes.scanline_start_cycle + (x * PPU_CLOCK_DIVIDER);
            nes.sprite0_detected = 1;
            if (trace_ppu_writes) {
                nes_printtime();
                printf("Sprite 0 hit at x=%i!\n", x);
            }
        }

        for (i=1; i<num_sprites; i++) {
            byte tmp = sprite_unit_clock(sprites+i);
            if (!(sprite_output & 3)) sprite_output = tmp;
        }

        if (sprite_output & 3) {
            if (sprite_output & 0x10) { /* Background sprite? */
                if (!(background & 0x40)) combined_output = nes.ppu.vram[0x3F10 + (sprite_output & 15)];
            } else combined_output = nes.ppu.vram[0x3F10 + (sprite_output & 15)];
        }

        if (!(nes.ppu.control2 & 4) && (x < 8)) combined_output = background;
        dest[x] = combined_output;
    }
    
}

static inline word incr_v_horizontal (word v)
{
    if ((v & 0x1F) != 0x1F) return v+1;
    else return v ^ 0x41F;
}

static inline word incr_v_vertical (word v)
{
    if ((v >> 12) < 7) return v + (1<<12);
    else {
        v &= (1<<12)-1;
        if ((v & 0x3E0) == 0x3A0) return v ^ 0xBA0;
        else { 
            /* For most games you can just return v + 32 here, but you
               can't let the increment from bits 5-9 carry into bit 10
               in the case where the game wrote a value >239 as the
               vertical scroll, e.g. Arkista's Ring.
             */
            unsigned sum = (v & 0x3E0) + (1<<5);
            word newv = ((v & ~0x3E0) | (sum & 0x3E0));
            return newv;
        }
    }
}

static inline word nametable_read (word v)
{
    return nes.ppu.vram[ppu_mirrored_nt_addr(0x2000 | (v & 0xFFF))];
}

static inline word name_to_attr_address (word n)
{
    word rel = n & 0x3FF;
    word base = (n & 0x0C00) | 0x3C0; /* WWNESD? */
    byte xchunk = (rel/4) & 7, ychunk = (rel>>7) & 7;
    return 0x2000 | base | xchunk | (ychunk << 3);
}

static inline byte nt_attr (word nt_addr)
{
    byte a = nes.ppu.vram[ppu_mirrored_nt_addr(name_to_attr_address(nt_addr))];
    if (nt_addr & 2) a >>= 2;
    if (nt_addr & 64) a >>= 4;
    return a & 3;
}

void ensure_tilecache (void)
{
    if (vid_tilecache_dirty) {
        gen_page (nes.ppu.vram, 0);
        gen_page (nes.ppu.vram + 0x1000, 1);
        vid_tilecache_dirty = 0;
    }
}

void render_scanline (void)
{
    byte *dest = color_buffer;
    byte *start = dest;
    word v = nes.ppu.v;
    unsigned y_offset = v >> 12;
    unsigned x_offset = nes.ppu.x;
    int tcpage = (nes.ppu.control1 & 0x10) >> 4;
    int i;
    byte name, attribute, *data;

    /* I'll have to ditch this if I want to run anywhere that requires aligned accesses: */
    unsigned int *destl;

    if (nes.ppu.control2 & 8) {
        ensure_tilecache();
        if (x_offset) {
            name = nametable_read(v);
            attribute = nt_attr((v & 0xFFF) | 0x2000);
            data = tilecache[tcpage][name][attribute] + 8*y_offset + x_offset;
            for (i = x_offset; i < 8; i++) *dest++ = *data++;
            v = incr_v_horizontal(v);
        }
        
        destl = dest;
        for (i=0; i<31; i++) {
            unsigned int *tdata;
            name = nametable_read(v);
            attribute = nt_attr((v & 0xFFF) | 0x2000);
            tdata = &tilecache[tcpage][name][attribute][8*y_offset];
            *destl++ = *tdata++;
            *destl++ = *tdata++;
            v = incr_v_horizontal(v);
        }
        dest += 248;

        name = nametable_read(v);
        attribute = nt_attr((v & 0xFFF) | 0x2000);
        data = tilecache[tcpage][name][attribute] + 8*y_offset;
        for (i = 0; i < (x_offset? x_offset : 8); i++) *dest++ = *data++;

    } else {
        //for (i=0; i<33; i++) v = incr_v_horizontal(v);
        memset(dest, nes.ppu.vram[0x3F00], 256);
    }

    if (!(nes.ppu.control2 & 2)) memset(start, nes.ppu.vram[0x3F00], 8);

    if (nes.ppu.control2 & 0x10) scanline_render_sprites(start);

    /* TV Borders - erase top 7 and bottom 8 lines */
    if ((tv_scanline < 7) || (tv_scanline >= 232)) memset(start, 63, 256);

    if (nes.ppu.control2 & 0x18) {
        v = incr_v_vertical(v);
        nes.ppu.v = v;
    }

    emphasis_position = 0;
}

void catchup_emphasis_to_x (int x)
{
    assert(emphasis_position <= x);
    assert(emphasis_position >= 0);
    if (x > 255) x = 255;
    unsigned dx = x - emphasis_position;
    if (dx) memset(emphasis_buffer + emphasis_position, nes.ppu.control2, dx);
    emphasis_position = x;
}


void catchup_emphasis (void)
{
    unsigned tmp = (nes.cpu.Cycles - nes.scanline_start_cycle) / PPU_CLOCK_DIVIDER;

    // Tiny timing kludge. Need to research this.
    tmp -= 12;

    if (emphasis_position < 0) return;
    tmp = min(255, tmp);
    assert(tmp >= 0);
    catchup_emphasis_to_x(tmp);
}


