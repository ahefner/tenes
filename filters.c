#include "nespal.h"
#include "filters.h"
#include "global.h"

/* I've disabled scale2x until I can sort out the licenses. The scaler
 * code is GPL, but M6502 is incompatible with the GPL. My current
 * preference is to drop M6502 so I can GPL this and enable the
 * scaler, or drop them both and license it MIT-style.
 */

/********************************************
#include "scale2x.h"
#if defined(__GNUC__) && defined(__i386__)
#define scale2x scale2x_8_mmx
#else
#define scale2x scale2x_8_def
#endif
*********************************************/

unsigned rgb_palette[64];
unsigned grayscale_palette[64];

void build_color_maps (void)    
{
    for (int i=0; i<64; i++) {
        rgb_palette[i] = (nes_palette[i*3] << 16) | (nes_palette[i*3+1] << 8) | nes_palette[i*3+2];
        grayscale_palette[i] = (nes_palette[i*3] + nes_palette[i*3+1] + nes_palette[i*3+2]) / 3;        
    }
}

static inline unsigned convert_pixel (byte color, byte emphasis)
{
    emphasis &= 0xE1;
    if (!emphasis) return rgb_palette[color & 63];
    else {
        if (emphasis & 1) color &= 0x30;
        unsigned px = rgb_palette[color & 63];
        byte r = px >> 16, g = (px >> 8) & 0xFF, b = px & 0xFF;
        // This is almost certainly very wrong.
        if (emphasis & 0x20) { g = g*3/4; b = b*3/4; }
        if (emphasis & 0x40) { r = r*3/4; b = b*3/4; }
        if (emphasis & 0x80) { r = r*3/4; g = g*3/4; }        
        return (r << 16) | (g << 8) | b;
    }
}

void no_filter_emitter (unsigned y, byte *colors, byte *emphasis)
{
    Uint32 *dest = (Uint32 *)(((byte *)window_surface->pixels) + y * window_surface->pitch);
    for (int x = 0; x < 256; x++) *dest++ = convert_pixel(colors[x], emphasis[x]);
}

void no_filter (void)
{
    vid_width = 256;
    vid_height = 240;
    vid_bpp = 32;
    filter_output_line = no_filter_emitter;
    build_color_maps();
}

void rescale_2x_emitter (unsigned y, byte *colors, byte *emphasis)
{
    Uint32 *dest0 = ((byte *)window_surface->pixels) + (y*2) * window_surface->pitch;
    Uint32 *dest1 = ((byte *)window_surface->pixels) + (y*2+1) * window_surface->pitch;
    for (int x = 0; x < 256; x++) {
        Uint32 px = convert_pixel(colors[x], emphasis[x]);
        *dest0++ = px; 
        *dest0++ = px;
        *dest1++ = px; 
        *dest1++ = px;
    }
}

void rescale_2x (void)
{
    vid_width = 512;
    vid_height = 480;
    vid_bpp = 32;
    filter_output_line = rescale_2x_emitter;
    build_color_maps();
}
