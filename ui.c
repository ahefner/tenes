#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define NES_UI_C

#include "sys.h"
#include "nes.h"
#include "sound.h"
#include "vid.h"
#include "config.h"
#include "nespal.h"
#include "font.h"
#include "filesystem.h"
#include "ui.h"

#include <SDL/SDL_image.h>


void (*menu) (struct inputctx *) = NULL;


/*** Freetype glue ***/

FT_Library ftlibrary;
FT_Face face_sans;
int freetype_init = 0;

int ensure_freetype (void)
{
    if (!freetype_init) {
        freetype_init = -1;
        int error = FT_Init_FreeType(&ftlibrary);
        if (error) {
            fprintf(stderr, "Unable to initialize freetype.\n");
            return -1;
        }

        char *filename = asset("DejaVuSans.ttf");
        error = FT_New_Face(ftlibrary, filename, 0, &face_sans);
        if (error) {
            fprintf(stderr, "Error opening %s\n", filename);
            return -1;
        }

        freetype_init = 1;
    }

    return freetype_init;
}


image_t sans_label (Uint32 color, unsigned text_height, const char *string)
{
    static byte *rtmp = NULL;
    int rwidth = window_surface->w;
    static unsigned tmpheight = 0;
    static unsigned rheight = 0;
    int baseline = text_height + 4;
    int error;
    int first_char = 1;
    int min_x = 0;
    int min_y = baseline;
    int max_y = baseline + 1;
    int max_x = 1;
    
    assert(text_height > 0);
    if (text_height > tmpheight) {
        if (rtmp) free(rtmp);
        // This is what you might call a hack.
        rheight = (text_height*2 + 4);
        rtmp = calloc(1, rwidth * rheight);
        assert(rtmp != NULL);
        tmpheight = text_height;
    } else memset(rtmp, 0, rwidth * rheight);

    FT_Set_Pixel_Sizes(face_sans, 0, text_height);

    int pen_x = 0;
    FT_UInt last_glyph_index = 0;

    char *str = string;
    for (char c = *str++; c; c=*str++) {
        // Beg Freetype to render the glyph
        FT_UInt glyph_index = FT_Get_Char_Index(face_sans, c); 

        if (first_char) first_char = 0;
        else {
            FT_Vector delta;
            FT_Get_Kerning(face_sans, last_glyph_index, glyph_index, 
                           FT_KERNING_DEFAULT, &delta);
            pen_x += delta.x >> 6;
            last_glyph_index = glyph_index;
        }

        error = FT_Load_Glyph(face_sans, glyph_index, 
                              FT_LOAD_RENDER | 
                              FT_LOAD_NO_HINTING | 
                              FT_LOAD_TARGET_LIGHT);
        if (error) continue;

        FT_Bitmap *bmp = &face_sans->glyph->bitmap;
        /*
        printf("  x=%3i: Char %c:   %ix%i + %i + %i   advance=%f\n",
               pen_x, c, bmp->width, bmp->rows,
               face_sans->glyph->bitmap_left,
               face_sans->glyph->bitmap_top,
               ((float)face_sans->glyph->advance.x) / 64.0);
        */        

        /* Transfer the glyph to the temporary buffer.. */
        int ox0 = pen_x + face_sans->glyph->bitmap_left;
        int ix0 = 0;
        int ox1 = ox0 + bmp->width;
        int ix1 = bmp->width;

        // Clip to left edge
        if (ox0 < 0) {
            printf("PRECLIP %s\n", string);
            ix0 -= ox0;
            ox0 -= ox0;
        }

        if (ox0 >= rwidth) break;

        // Clip to right edge
        int overflow = max(0, ox1 - rwidth);
        if (overflow) printf("POSTCLIP\n");
        ox1 -= overflow;
        ix1 -= overflow;
        int width = ox1 - ox0;
        //printf("Compute width \"%s\" char '%c' -- %i - %i = %i\n", string, c, ox1, ox0, width);
        assert(width >= 0);
        assert(ix0 >= 0);

        int oy0 = baseline - face_sans->glyph->bitmap_top;
        int height = bmp->rows;

        // Update bounding rectangle
        //min_x = min(min_x, cx0);
        max_x = max(max_x, ox1);
        min_y = max(0, min(min_y, oy0));
        max_y = min(rheight, max(max_y, oy0+height));

        // Ick?
        unsigned char *input = bmp->buffer;
        int input_pitch = bmp->pitch;
        for (int y=0 ; y<height; y++) {
            int y_out = oy0 + y;
            if ((y_out >= 0) && (y_out < rheight)) {
                byte *ptr = rtmp + y_out*rwidth;
                unsigned char *row = input + ix0 + y*input_pitch;

                for (int xoff=0; xoff<(ox1-ox0); xoff++) {
                    int tmp = ptr[ox0+xoff];
                    tmp += row[xoff];
                    ptr[ox0+xoff] = clamp_byte(tmp);
                }
            } else printf("    fuck off at %i\n", y);
        }

        pen_x += face_sans->glyph->advance.x >> 6;
    }

    Uint32 *data = malloc(4*(max_x - min_x)*(max_y - min_y));

    if (data == NULL) {
        printf("Can't allocate final memory for size %i label \"%s\"\n", text_height, string);
        printf("Final bounds: %i %i %i %i baseline=%i\n", 0, min_y, max_x, max_y, baseline);
        return NULL;
    } else {
        image_t img = malloc(sizeof(*img));
        assert(img);
        int w = max_x - min_x;

        for (int y=min_y; y<max_y; y++)
            for (int x=min_x; x<max_x; x++)
                data[(y-min_y)*w+(x-min_x)] = color | (rtmp[y*rwidth+x] << 24);

        img->w = w;
        img->h = max_y - min_y;
        img->x_origin = 0;
        img->y_origin = baseline - min_y;
        img->freeptr = data;
/*
        img->x_origin = -((img->w*align_x[ALIGN_MAX]) >> align_x[ALIGN_MAX_SHIFT]);
        img->y_origin = -((img->h*align_y[ALIGN_MAX]) >> align_y[ALIGN_MAX_SHIFT])
            - (min_y - baseline)*align_y[ALIGN_AXIS];
*/
        img->_sdl = SDL_CreateRGBSurfaceFrom(data, img->w, img->h, 32, img->w*4,
                                             0xFF0000, 0xFF00, 0xFF, 0xFF000000);
        assert(img->_sdl);
        return img;
    }
}



/*** UI Utilities ***/

unsigned cursor_base[2] = {7,20};
unsigned cursor[2] = {0,0};

void setcursor (int col, int row) 
{
    cursor[0] = col * text_width + cursor_base[0];
    cursor[1] = (row+1) * text_height + cursor_base[1];
}

const Uint32 color00 = 0xFFFFFF;
const Uint32 color01 = 0x888888;
const Uint32 color10 = 0xFFBB88;
const Uint32 color20 = 0xCCFFAA;

Uint32 cursor_color = 0xFFFFFF;

void setcolor (Uint32 color) { cursor_color = color; }

SDL_Rect print (char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[8192];
    int xmin = cursor[0], xmax = cursor[0];
    int ymin = cursor[1], ymax = cursor[1];

    vsnprintf(buf, sizeof(buf), fmt, args);
    buf[8191] = 0;
    char *ptr = buf;

    while (*ptr) {
        char *next = strchr(ptr, '\n');
        if (next) *next = 0;
        cursor[0] = drop_string(cursor[0], cursor[1], ptr, cursor_color);
        xmin = min(xmin, cursor[0]);
        xmax = max(xmax, cursor[0]);
        ymin = min(ymin, cursor[1] - text_ascent);
        ymax = max(ymax, cursor[1] + text_descent);
        
        if (next) {
            cursor[0] = cursor_base[0];
            cursor[1] += text_height;
            ptr = next+1;
        } else break;
    }

    va_end(args);

    return (SDL_Rect){xmax-xmin, ymax-ymin, xmin, ymin};
}

void dim_to_y (int y)
{
    for (int i=0; i<min(y, window_surface->h); i++) {
        Uint32 *ptr = display_ptr(0, i);
        unsigned width = window_surface->w;
        for (int x=0; x<width; x++) ptr[x] = (ptr[x] >> 2) & 0x3F3F3F;
    }
}

float dim_y = 0.0;
float dim_y_target = 0.0;

void dim_background (void)
{
    float rate = 0.13;
    dim_y = dim_y * (1.0-rate) + rate*dim_y_target;
    dim_to_y(floor(dim_y));
}

//typedef SDL_Surface *image_t;



char *asset (char *name)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "media/%s", name);
    buf[511] = 0;
    return buf;
}

char *nth_name (char *name, int n)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s_%i.png", name, n);
    buf[511] = 0;
    return buf;
}

image_t loaddecal (char *name)
{
    image_t img = NULL;
    SDL_Surface *ptr = IMG_Load(asset(name));

    if (ptr) {
        SDL_SetAlpha(ptr, SDL_SRCALPHA, 128);
        /* Naughty! Twiddle image into ARGB format. */
        ptr->format->Amask = 0xFF000000;
        ptr->format->Ashift = 24;
        img = malloc(sizeof(*img));
        assert(img);
        img->w = ptr->w;
        img->h = ptr->h;
        img->x_origin = 0;
        img->y_origin = 0;
        img->_sdl = ptr;
        img->freeptr = NULL;
    } else printf("Unable to load %s!\n", asset(name));

    return img;
}

image_t pad600 = NULL;
image_t mascot = NULL;
image_t drop_roundbutton = NULL;
image_t power_lo[2] = { NULL, NULL };
image_t reset_lo[2] = { NULL, NULL };
image_t keyboard_140 = NULL;
image_t keyboard_280 = NULL;
image_t cableseg = NULL;
image_t nesport[2] = { NULL, NULL };
image_t nesport_shadow = NULL;
image_t photo = NULL;
image_t photo_shadow_left = NULL;
image_t photo_shadow_bottom = NULL;
image_t square_shadow = NULL;
image_t camera[2] = { NULL, NULL };
image_t eject[2] = { NULL, NULL };
image_t sound[2] = { NULL, NULL };
image_t mute[2] = { NULL, NULL };
image_t gamepak_top = NULL;
image_t border_top_1 = NULL;
image_t scroll_outer_top = NULL;
image_t scroll_outer_bottom = NULL;
image_t scroll_inner_top = NULL;
image_t scroll_inner_bottom = NULL;
image_t device_bar_bg = NULL;
image_t rcol_top = NULL;
image_t rcol_middle = NULL;
image_t rcol_bottom = NULL;
image_t checkmark = NULL;


#define decal(name) if (!name) name = loaddecal(#name".png");
#define decals(name) \
  { for (int i=0; i<(sizeof(name)/sizeof(name[0])); i++) \
     name[i] = loaddecal(nth_name(#name, i)); }

void load_menu_media (void)
{
    decal(pad600);
    decal(mascot);
    decals(power_lo);
    decals(reset_lo);
    decal(drop_roundbutton);
    decal(keyboard_140);
    decal(keyboard_280);
    decal(cableseg);
    decals(nesport);
    decal(nesport_shadow);
    decal(photo);
    decal(photo_shadow_left);
    decal(photo_shadow_bottom);
    decal(square_shadow);
    decals(camera);
    decals(eject);
    decals(sound);
    decals(mute);
    decal(gamepak_top);
    decal(border_top_1);
    decal(scroll_outer_top);
    decal(scroll_outer_bottom);
    decal(scroll_inner_top);
    decal(scroll_inner_bottom);
    decal(device_bar_bg);
    decal(rcol_top);
    decal(rcol_middle);
    decal(rcol_bottom);
    decal(checkmark);
}

const int ALIGN_MAX = 0;
const int ALIGN_MAX_SHIFT = 1;
const int ALIGN_AXIS = 2;

alignmode left     = {  0, 0, 0};
alignmode right    = { -1, 0, 0};
alignmode center   = { -1, 1, 0};
alignmode top      = {  0, 0, 0};
alignmode bottom   = { -1, 0, 0};
alignmode baseline = {  0, 0,-1};

SDL_Rect drawimage (image_t image, int x, int y, 
                    alignmode align_x, 
                    alignmode align_y)
{
    if (image) {
        SDL_Rect r = { x + align_x[ALIGN_MAX]*(image->w >> align_x[ALIGN_MAX_SHIFT]) 
                       + align_x[ALIGN_AXIS]*image->x_origin,
                       y + align_y[ALIGN_MAX]*(image->h >> align_y[ALIGN_MAX_SHIFT])
                       + align_y[ALIGN_AXIS]*image->y_origin,
                       0, 0 };
        SDL_BlitSurface(image->_sdl, NULL, window_surface, &r);
        return r;
    }
    return (SDL_Rect){0, 0, 0, 0};
}

int mouseover (struct inputctx *input, SDL_Rect rect) {
    return ((input->mx >= rect.x) && 
            (input->my >= rect.y) &&
            (input->mx < rect.x+rect.w) &&
            (input->my < rect.y+rect.h));
}

SDL_Rect grow_rect (int n, SDL_Rect rect)
{
    return (SDL_Rect){rect.x - n, rect.y - n, rect.w + 2*n, rect.h + 2*n};
}

float lerpf (float x, float arg, float y)
{
    return x*arg + y*(1.0-arg);
}

struct floatybutt {
    float offset;
    int hover;
};

const float floaty_rate = 0.25;
const float floaty_height = 10;
const float floaty_linear = 0.1; // Hmm. Don't like this.

int run_floatybutt (struct inputctx *input, struct floatybutt *this, 
                    image_t faces[2], image_t shadow, int x, int y)
{
    this->hover = mouseover(input, drawimage(shadow, x, y, left, bottom));
//    if (this->hover && (input->buttons & 1)) this->offset = lerpf(0, floaty_rate, this->offset);
//    else this->offset = lerpf(floaty_height, floaty_rate, this->offset);
    if (this->hover && (input->buttons & 1)) this->offset = approach(0, floaty_linear, floaty_rate, this->offset);
    else this->offset = approach(floaty_height, floaty_linear, floaty_rate, this->offset);

    drawimage(faces[this->hover], x+this->offset, y-this->offset, left, bottom);
    
    return (this->hover && (input->released & 1));    
}

float normsq (float x, float y)
{
    return x*x + y*y;
}

void draw_cable (float px, float py, float destx, float desty)
{
    float dx=0, dy=8.0;

    int segments=0;
    while ((segments < 1000) && (normsq(px-destx, py-desty) > 1.0)) {
        drawimage(cableseg, px, py, center, center);
        float vx = destx - px, vy = desty - py;
        float rate = 1.0 / sqrt(normsq(vx, vy));
        dx += vx*rate;
        dy += vy*rate;
        //printf("%i: %5f,%4f %5f,%4f \n", segments, px, dx, py, dy);
        float ns = normsq(dx,dy);
        if (ns > 6.0) {
            dx /= 2.0;
            dy /= 2.0;
        }
        dy += 0.5;
        px += dx;
        py += dy;
        segments++;
    }
}

void age_pixels (Uint32 *ptr, size_t n)
{
    for (int i=0; i<n; i++) {
        Uint32 px = ptr[i];
        px &= 0xFCFCFC;
        int level = ((px + (px >> 8) + (px >> 16)) >> 2) & 0xFF;
        level += random()&31;
        level += 31;
        ptr[i] = rgbi_clamp(level+30, level+30, level);
    }
}

/*** Menu dispatch ***/

void run_menu (struct inputctx *input)
{
    cursor_base[0] = 10;
    cursor_base[1] = 40;
    setcursor(0,0);
    setcolor(color00);

    menu(input);
}

void open_menu (void)
{
    load_menu_media();
    menu = run_main_menu;
    //menu = run_input_menu;
    SDL_ShowCursor(SDL_ENABLE);
}

void close_menu (void)
{
    menu = 0;
    SDL_ShowCursor(SDL_DISABLE);
    dim_y_target = 0;
}

int menu_process_key_event (SDL_KeyboardEvent *key)
{
    if (key->type == SDL_KEYDOWN) return 0;
    
    if (key->keysym.sym == SDLK_ESCAPE) {
        close_menu();
        return 1;
    }
    
    return 0;
}

/*** Main menu screen ***/

char last_state_filename[512] = "";
time_t last_state_mtime = 0;
byte state_screencap[2][SCREEN_HEIGHT][SCREEN_WIDTH];
Uint32 rgb_screencap[SCREEN_HEIGHT][SCREEN_WIDTH];
Uint32 aged_screencap[SCREEN_HEIGHT][SCREEN_WIDTH];
int have_screencap = 0;

void update_state_image (void)
{
    char *filename = state_filename(&nes.rom, 1);
    time_t mtime = file_write_date(filename);
    char buf[512];

    if (!strcmp(filename, last_state_filename) && 
        (mtime == last_state_mtime)) return;
    
    last_state_mtime = mtime;
    strncpy(last_state_filename, filename, sizeof(last_state_filename));

    snprintf(buf, sizeof(buf), "%s.screen", filename);
    have_screencap = load_binary_data(buf, state_screencap, sizeof(state_screencap));
    if (have_screencap) {
        for (int i=0; i<SCREEN_HEIGHT; i++) {
            emit_unscaled(&rgb_screencap[i][0], 
                          &state_screencap[0][i][0], 
                          &state_screencap[1][i][0],
                          SCREEN_WIDTH) ;            
        }
        memcpy(aged_screencap, rgb_screencap, sizeof(rgb_screencap));
        age_pixels(&aged_screencap[0][0], SCREEN_WIDTH*SCREEN_HEIGHT);
    }
    printf("Updated screencap %s\n", buf);
}

void render_restorestate (struct inputctx *input)
{   
    const int photo_border = 10;
    const int overscan = 8;
    static int fade = 255;
    
    int cx = vid_width - 255 - 27;
    int cy = 10 + photo->h + 10;
    
    const float button_height = 7;
    static float offset = 7;    /* const is dumb */
    const int actual_width = SCREEN_WIDTH - 2*overscan;
    const int actual_height = SCREEN_HEIGHT-2*overscan;
    int px = cx + 3;
    int py = cy - 3;
    int vx = px + roundf(offset) + photo_border;
    int vy = py - roundf(offset) - photo->h + photo_border + 1;

    if (probe_file(state_filename(&nes.rom, 1))) {
        update_state_image();       

        drawimage(photo_shadow_left, cx, cy, left, bottom);
        drawimage(photo_shadow_bottom, cx+photo_shadow_left->w, cy, left, bottom);

        int hover = mouseover(input, grow_rect(1.4*ceilf(button_height-offset), drawimage(photo, px + roundf(offset), py-roundf(offset), left, bottom)));

        float target = (hover && (input->buttons & 1))? 0.0 : button_height;
        float d_offset = 0.05 + 0.5 * fabs(target - offset);
        
        if (hover && (input->buttons & 1)) d_offset *= -1.0;
        offset = min(button_height, max(0, offset + d_offset));

        int d_fade = hover? -10: 10;
        fade = clamp_byte(fade + d_fade);
        int notfade = 255-fade;

        if (have_screencap) {
            for (int i=0; i<actual_height; i++) 
            {
                Uint32 *ptr = display_ptr(vx, vy+i);
                Uint32 *aged = &aged_screencap[i+overscan][overscan];
                Uint32 *rgb = &rgb_screencap[i+overscan][overscan];
                if (fade==255) memcpy(ptr, aged, 4*actual_width);                
                else for (int x = 0; x<actual_width; x++) {
                        ptr[x] = rgbi((fade*(RED(aged[x]))   + notfade*(RED(rgb[x]))) >> 8,
                                      (fade*(GREEN(aged[x])) + notfade*(GREEN(rgb[x]))) >> 8,
                                      (fade*(BLUE(aged[x]))  + notfade*(BLUE(rgb[x]))) >> 8);
                    }
            }
        }

        vy += actual_height;
        vy += 8;

        char buf[64];
        struct tm  *ts;
        ts = localtime(&last_state_mtime);
        strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", ts);
        if (hover) strcpy(buf, "Restore State");
        outlined_string(vx + actual_width/2 - strlen(buf)*text_width/2, 
                        vy + text_ascent, buf, 0x79786d /*0x65645b*/, 0xbab7a7);
        vy += text_height + 2;
        dim_y_target = max(dim_y_target, vy);

        if (hover && (input->released & 1)) { 
            if (!restore_state_from_disk(NULL)) reset_nes(&nes);
        }
    } else {
        vy += actual_height + 8 + text_height + 2;
    }
}


void run_main_menu (struct inputctx *input)
{   
    //drawimage(pad600, 0, 0, left, top);
    //drawimage(mascot, window_surface->w, window_surface->h - 80, right, bottom);
    
    if (input->released & SDL_BUTTON(3)) close_menu();

    cursor_base[0] = 10;
    cursor_base[1] = 415;
    setcursor(0,0);
    setcolor(color00);
    print("This space for rent.\n");

    setcolor(color10);
    print("Playing: %s\n", nes.rom.title);
    print("Size: %i KB Program,  %i KB Graphics\n", nes.rom.prg_size/1024, nes.rom.chr_size/1024);
    print("Mapper %i: %s\n", nes.rom.mapper, nes.rom.mapper_info? nes.rom.mapper_info->name : "Unknown");
//    if (sound_globalenabled) print("Sound %s\n", sound_muted? "muted" : "enabled");
    if (cfg_disable_joysticks) print("Joysticks disabled.\n");
    if (cfg_disable_keyboard) print("Keyboard input disabled.\n");
    if (movie_input_filename && movie_input) print("Playing movie from %s\n", movie_input_filename);
    if (movie_output_filename && movie_output) print("Recording movie to %s\n", movie_output_filename);
    if (cfg_mount_fs) print("FUSE filesystem mounted at \"%s\"\n", cfg_mountpoint);

/*
    setcolor(color00);
    print("\nInput Devices:\n");
    setcolor(color20);
    print("  Bitchin' Keyboard %c\n", 1);
    for (int i=0; i<numsticks; i++) {
        print("  %s w/ %i bodacious buttons and %i awesome axes\n", 
              SDL_JoystickName(i), 
              SDL_JoystickNumButtons(joystick[i]),
              SDL_JoystickNumAxes(joystick[i]));        
    }
*/
    render_restorestate(input);

    int icon_pad = 12;
    int icon_x = 0;
    int icon_y = 85; //window_surface->h - icon_pad;
    float vert_x = icon_x;
    float vert_y = icon_y;

    static struct floatybutt power = {0, 0};
    if (run_floatybutt(input, &power, power_lo, drop_roundbutton, icon_x, icon_y)) running = 0;
    icon_x += icon_pad + power_lo[0]->w;
    vert_y += icon_pad + power_lo[0]->h;

    static struct floatybutt reset = {0, 0};
    if (run_floatybutt(input, &reset, reset_lo, drop_roundbutton, icon_x, icon_y)) reset_nes(&nes);
    icon_x += icon_pad + reset_lo[0]->w;
    icon_x += icon_pad;

    static struct floatybutt eject_button = {0, 0};
    if (run_floatybutt(input, &eject_button, eject, square_shadow, vert_x, vert_y)) menu = run_game_browser;
    vert_y += icon_pad + eject[0]->h;

    static struct floatybutt camera_button = {0, 0};
    if (run_floatybutt(input, &camera_button, camera, square_shadow, vert_x, vert_y)) save_state_to_disk(NULL);
    vert_y += icon_pad + camera[0]->h;

    static struct floatybutt mute_button = {0, 0};
    if (sound_globalenabled) {
        if (run_floatybutt(input, &mute_button, sound_muted? mute : sound, 
                           square_shadow, 
                           vert_x, vert_y)) 
            sound_muted = !sound_muted;
        vert_y += icon_pad + camera[0]->h;
    }

    //drawimage(keyboard_140, input->mx, input->my, right, bottom);

    int port_y = window_surface->h - icon_pad - 6;
    static struct floatybutt port_button = {0,0};
    if (run_floatybutt(input, &port_button, nesport, nesport_shadow, 
                       window_surface->w - 29 - nesport[0]->w, port_y)) 
        menu = run_input_menu;
    //drawimage(nesport, window_surface->w - 8, port_y, right, bottom);
    //drawimage(nesport, window_surface->w - 10 - nesport->w, port_y, right, bottom);

    // Clever stunt:
/*
    static float destx = 500, desty= 65;
    if (input->pressed == 2) {
        destx = input->mx;
        desty = input->my;
    }
    draw_cable(input->mx, input->my, destx, desty);
    drawimage(keyboard_140, destx, desty, center, center);
*/

    //dim_y_target = max(dim_y_target, cursor[1] + text_height);
    dim_y_target = vid_height;
}


/*** Game Browser ***/

struct gbent
{
    char *title;
    char *name;
    char *path;
    image_t background;    /* Not freed! */
    image_t label;
    image_t highlight;
    float xoffset, xtarget;
    unsigned y;
    int hover;
};

struct gbent **browser_ents;
unsigned browser_num_ents = 0;
unsigned browser_max_ents = 0;
char browser_cwd[1024] = "";
image_t browser_dir_label = NULL;
image_t browser_dir_shadow = NULL;

void free_gbent (struct gbent *ent) {
    assert(ent != NULL);
    free(ent->name);
    free(ent->path);
    free(ent->title);
    if (ent->label) image_free(ent->label);
    if (ent->highlight) image_free(ent->highlight);
    free(ent);
}

struct gbent *browser_dirs[1024];
unsigned browser_num_dirs = 0;

int game_filename_p (char *filename)
{
    int len = strlen(filename);
    if (len < 5) return 0;
    return !strcasecmp(filename+len-4, ".nes");
}

char *filename_to_title (char *filename)
{
    char *out = strdup(filename);
    char *tmp = strrchr(out, '.');
    if (tmp) *tmp = 0;
    return out;
}

int browser_ent_title_relation (struct gbent **a, struct gbent **b)
{
    return strcasecmp((*a)->title, (*b)->title);
}

int trailing_slash_p (char *str)
{
    return strlen(str) && (str[strlen(str)-1] == '/');
}

const char *dirsep (char *path)
{
    return trailing_slash_p(path)? "" : "/";
}


struct breadcrumb {
    image_t label, highlight, shadow;
    char name[PATH_MAX];
    char path[1024];
    struct breadcrumb *next;
    int use_highlight;
};

struct breadcrumb *browser_breadcrumbs;

void free_breadcrumb (struct breadcrumb *b)
{
    if (b->label) image_free(b->label);
    if (b->highlight) image_free(b->highlight);
    if (b->next) free_breadcrumb(b->next);
    free(b);
}

struct breadcrumb *build_breadcrumbs (char *path)
{
    struct breadcrumb *root = calloc(1, sizeof(*root));
    assert(root != NULL);
    strcpy(root->name, "Viewing /");
    strcpy(root->path, "/");

    char *ptr = path + strspn(path, "/");
    struct breadcrumb *tail = root;

    while (*ptr) {        
        struct breadcrumb *next = calloc(1, sizeof(*next));
        assert(next != NULL);
        char *sep = strchrnul(ptr, '/');
        if (*sep == '/') sep++;
        memcpy(next->name, ptr, min(sizeof(next->name)-1, sep-ptr));
        memcpy(next->path, path, min(sizeof(next->path)-1, sep-path));
        ptr = sep + strspn(sep, "/");
        tail->next = next;
        tail = next;
    }
    
    return root;
}

void browser_rescan (void)
{
    assert(strlen(browser_cwd));
    save_pref_string("browser_cwd", browser_cwd);

    for (int i=0; i<browser_num_ents; i++) free_gbent(browser_ents[i]);
    browser_num_ents = 0;

    for (int i=0; i<browser_num_dirs; i++) free_gbent(browser_dirs[i]);
    browser_num_dirs = 0;
    int max_dirs = sizeof(browser_dirs)/sizeof(browser_dirs[0]);
    
    DIR *dir = opendir(browser_cwd);

    if (dir == NULL) {
        perror("opendir");
        strcpy(browser_cwd, "/");
        dir = opendir(browser_cwd);
        if (!dir) return;
    }

    if (browser_breadcrumbs) free_breadcrumb(browser_breadcrumbs);
    browser_breadcrumbs = build_breadcrumbs(browser_cwd);

    struct dirent *dent;

    while ((dent = readdir(dir))) {
        char filename[1024];        
        snprintf(filename, sizeof(filename), "%s%s%s", 
                 browser_cwd, dirsep(browser_cwd), dent->d_name);
        
        struct stat st;
        int exists = !stat(filename, &st);

        if (exists && S_ISREG(st.st_mode) &&
            game_filename_p(filename)) 
        {
            /* Add file to browser */

            struct gbent *ent = calloc(1, sizeof(struct gbent));
            assert(ent != NULL);
            ent->name = strdup(dent->d_name);
            ent->path = strdup(filename);
            ent->title = filename_to_title(dent->d_name);
            ent->background = gamepak_top;

            assert(browser_num_ents <= browser_max_ents);
            if (browser_num_ents == browser_max_ents) {
                size_t newmax = 2*max(1,browser_max_ents);
                struct gbent **newvec = calloc(newmax, sizeof(struct gbent *));
                assert(newvec != NULL);
                if (browser_ents) {
                    memcpy(newvec, browser_ents, browser_max_ents * sizeof(struct gbent *));
                    free(browser_ents);
                }
                browser_ents = newvec;
                browser_max_ents = newmax;
            }
            browser_ents[browser_num_ents] = ent;
            browser_num_ents++;
        }
        else if (exists && S_ISDIR(st.st_mode) && (browser_num_dirs < max_dirs))
        {   /* Add directory to browser */
            snprintf(filename, sizeof(filename), "%s%s%s/", 
                     browser_cwd, dirsep(browser_cwd), dent->d_name);
            if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) continue;
            struct gbent *ent = calloc(1, sizeof(struct gbent));

            assert(ent != NULL);
            ent->name = strdup(dent->d_name);
            ent->path = strdup(filename);
            ent->title = strdup(dent->d_name);
            ent->label = NULL;
            ent->background = NULL;
            

            browser_dirs[browser_num_dirs++] = ent;
        }
    }

    closedir(dir);
    if (browser_dir_label) free(browser_dir_label);
    if (browser_dir_shadow) free(browser_dir_shadow);
    browser_dir_label = sans_label(color00, 24, browser_cwd);
    browser_dir_shadow = sans_label(0x000000, 24, browser_cwd);

    qsort(browser_ents, browser_num_ents, sizeof(browser_ents[0]), 
          (int(*)(const void *, const void *))browser_ent_title_relation);

    qsort(browser_dirs, browser_num_dirs, sizeof(browser_dirs[0]),
          (int(*)(const void *, const void *))browser_ent_title_relation);

    // We can't initialize the Y positions until we've sorted.
    for (int i=0; i<browser_num_ents; i++) browser_ents[i]->y = i * (gamepak_top->h - 5);
    for (int i=0; i<browser_num_dirs; i++) browser_dirs[i]->y = i * 20;
}

float approach (float target, float minrate, float rate, float current)
{
    float d = max(minrate, fabs((current-target) * rate));
    if (target < current) d = -d;
    return clampf(min(target,current), max(target,current), current+d);
}

void draw_hborder (int y, image_t image, alignmode align)
{
    int x = 0;
    assert(image->w > 0);
    while (x < window_surface->w) {
        drawimage(image, x, y, left, align);
        x += image->w;
    }
}

image_t render_gamepak_label (struct gbent *ent)
{
    const int max_width = 203;
    image_t label = sans_label(0x4c1017, 16, ent->title);
    if (label->w > max_width) {
        int oldw = label->w;
        image_free(label);
        label = sans_label(0x4c1017, 16.0 * ((float)max_width)/((float)oldw), ent->title);
    }

    return label;    
}

noinline void fill_rect (Uint32 color, int x0, int y0, int x1, int y1)
{
    int width = x1 - x0;
    x0 = clampi(0, x0, window_surface->w);
    x1 = clampi(0, x1, window_surface->w);
    y0 = clampi(0, y0, window_surface->h);
    y1 = clampi(0, y1, window_surface->h);
    assert(x0 <= x1);
    assert(y0 <= y1);
    for (unsigned y=y0; y<y1; y++) {
        Uint32 *ptr = display_ptr(x0, y);
        for (int x=0; x<width; x++) ptr[x] = color;
    }
}

void browser_set_path (char *path)
{
    printf("Set path to \"%s\"\n", path);
    save_pref_string("browser_cwd", path);
    browser_cwd[0] = 0;
}

void select_breadcrumb (struct breadcrumb *b)
{
    browser_set_path(b->path);
}

struct scrollbar 
{
    int grabbed;
    int grab_y_px;
    int grab_y_units;
};

#define Scrollbar(name) struct scrollbar name = { .grabbed = 0 };

int run_scrollbar (struct inputctx *input, 
                   struct scrollbar *scroll, 
                   int x, int s_top, int s_bottom, 
                   int y, int viewport_height, int max_y)
{
    int height = s_bottom - s_top;
    int max_scroll = max(0, max_y - viewport_height);
    float scroll_to_px = max_y? (float)height / (float)max_y : 0.0;
    float px_to_scroll = height? (float)max_y / (float)height : 0.0;

    float thumb_top = s_top + y * scroll_to_px;
    float thumb_bottom = s_top + (y + viewport_height) * scroll_to_px;

    fill_rect(0xFFFFFF, x, s_top, x + 16, s_bottom);
    drawimage(scroll_outer_top,    x, s_top, left, bottom);
    drawimage(scroll_outer_bottom, x, s_bottom, left, top);

    fill_rect(0x000000, x+1, thumb_top, x + 15, thumb_bottom);
    drawimage(scroll_inner_top, x, thumb_top, left, bottom);
    drawimage(scroll_inner_bottom, x, thumb_bottom, left, top);

    SDL_Rect rect = {.w=16, .h=height+16, .x=x, .y=s_top-8};

    if (scroll->grabbed) {
        int y_offset = input->my - scroll->grab_y_px;
        int new_y = clampi(0, max_scroll, scroll->grab_y_units + y_offset * px_to_scroll);
        if (!(input->buttons & 1)) scroll->grabbed = 0;
        input->released = input->released = 0;
        return new_y;
    } else if ((input->pressed & 1) && mouseover(input, rect)) {
        // Clicked within the scrollbar. Three possibilities:
        if (input->my <= (thumb_top-6)) {
            // Clicked above thumb
            return max(0, y - viewport_height);
        } else if (input->my > (thumb_bottom+6)) {
            // Clicked below thumb
            return min(max_scroll, y+viewport_height);
        } else {
            // Clicked in thumb. Grab pointer.
            scroll->grab_y_px = input->my;
            scroll->grab_y_units = y;
            scroll->grabbed = 1;
            input->pressed = 0;
        }
    } else if ((input->pressed & 2) && mouseover(input, rect)) {
        // Middle click, jump to position.
        return clampi(0, max_scroll, (input->my - s_top) * px_to_scroll);
    }

    return y;
}

void run_game_browser (struct inputctx *input)
{
    if (input->released & SDL_BUTTON(3)) menu = run_main_menu;
    dim_y_target = vid_height;

    int x_base = window_surface->w - 330;    /* Window vertical split  */
    static int game_scroll = 0;              /* Scrolling of game list */

    int y_top = 37;
    int viewport_height = window_surface->h - y_top;
    int scroll_top = y_top + 24;
    int scroll_bottom = window_surface->h - 24;

    int pointer_in_dir_pane = 0;
    int pointer_in_file_pane = 0;
    if (input->my > border_top_1->h) {
        if ((input->mx < x_base - 24 - 4)) pointer_in_dir_pane = 1;
        else pointer_in_file_pane = 1;
    }

    // Update current working directory
    if (!browser_cwd[0]) {
        char *pref = pref_string("browser_cwd", NULL);
        if (!pref) getcwd(browser_cwd, sizeof(browser_cwd));
        else strncpy(browser_cwd, pref, sizeof(browser_cwd));
        browser_rescan();
    }

    /** Draw directories **/
    static int dir_scroll = 0;
    static float dir_cur_scroll = 0.0;
    struct gbent *lastdir = browser_num_dirs? browser_dirs[browser_num_dirs-1] : NULL;
    int dir_height = lastdir? lastdir->y + 24 : 0;
    static Scrollbar(dir_scroller);

    if (dir_height >= viewport_height) {
        dir_scroll = run_scrollbar(input, &dir_scroller, 8, scroll_top, scroll_bottom, 
                                   dir_scroll, viewport_height, dir_height);
    } else dir_scroll = 0;

    int dir_wheel_step = 38;
    if (pointer_in_dir_pane && (input->pressed & 16)) dir_scroll += dir_wheel_step;
    if (pointer_in_dir_pane && (input->pressed & 8))  dir_scroll -= dir_wheel_step;
    dir_scroll = clampi(0, max(0, dir_height-viewport_height), dir_scroll);
    
    dir_cur_scroll = approach(dir_scroll, 2.0, 0.18, dir_cur_scroll);
    
    for (int i=0; i<browser_num_dirs; i++) {
        struct gbent *dir = browser_dirs[i];
        int label_height = 18;
        if (!dir->label) dir->label = sans_label(color10, label_height, dir->title);
        if (!dir->highlight) dir->highlight = sans_label(color00, label_height, dir->title);
        image_t label = dir->hover? dir->highlight : dir->label;
        SDL_Rect rect = drawimage(label, 33, y_top + label_height + dir->y - dir_cur_scroll, left, baseline);
        dir->hover = mouseover(input, rect);
        if (dir->hover && (input->released & 1)) {
            if (!strcmp(dir->name, "..")) {
                char *str = strdup(browser_cwd);
                if (trailing_slash_p(str)) str[strlen(str)-1] = 0;
                char *tmp = strrchr(str, '/');

                if (!tmp) browser_set_path("/");
                else {
                    *(tmp+1) = 0;
                    browser_set_path(str);
                }
                free(str);
            } else browser_set_path(dir->path);

                dir_scroll = 0;
                game_scroll = 0;
        }
        if (rect.y > window_surface->h) break;
    }

    /** Draw games **/
    static int game_cur_scroll = 0;
    static int game_pan = 0;
    static Scrollbar(file_scroller);
    int label_center = 163;
    struct gbent *last = browser_num_ents? browser_ents[browser_num_ents-1] : NULL;
    int max_y = last? last->y + last->background->h : 0;
    int pan_region_pad = 20;
    int pan_region_height = viewport_height - 2*pan_region_pad;

    // Mouse panning (is no more)
    /*
    if (input->mx >= x_base + 14) {
        float pan_rate = max(0.0, min(2.0, (max_y - viewport_height) / (float)pan_region_height));
        game_pan = (input->my - y_top - pan_region_pad) * pan_rate;
    }*/

    int already_found = 0;    

    int game_wheel_step = 64;
    if (pointer_in_file_pane && (input->pressed & 16)) game_scroll += game_wheel_step;
    if (pointer_in_file_pane && (input->pressed & 8))  game_scroll -= game_wheel_step;
    game_scroll = clampi(0, max_y - viewport_height, game_scroll);
    
    if (max_y < viewport_height) {
        game_scroll = 0;
    } else game_scroll = run_scrollbar(input, &file_scroller, x_base - 24, 
                                    scroll_top, scroll_bottom, 
                                    game_scroll, viewport_height, max_y);

    int combined_scroll = game_scroll + game_pan;
    combined_scroll = clampi(0, max(0,max_y-viewport_height), combined_scroll);

    game_cur_scroll = approach(combined_scroll, 4.0, 0.18, game_cur_scroll);

    for (int i=0; i<browser_num_ents; i++) {
        struct gbent *ent = browser_ents[i];
        int y = ent->y + y_top - game_cur_scroll;

        if ((y > -gamepak_top->h) && (y < window_surface->h)) {
            int x = x_base + roundf(ent->xoffset);
            SDL_Rect rect = drawimage(gamepak_top, x, y, left, top);
            rect.w -= floor(ent->xoffset);      // Otherwise, oscillates at right edge
            int over = mouseover(input, rect);

            if (!ent->label) ent->label = render_gamepak_label(ent); 

            drawimage(ent->label, x + label_center, y + 21, center, baseline);

            float pull_target = (window_surface->w - gamepak_top->w)/2 - x_base + label_center/2;
            if (!already_found && over && (input->my >= border_top_1->h) && !(file_scroller.grabbed))
            {
                if (input->buttons & 1) ent->xtarget = pull_target;
                if (input->released & 1) {
                    ent->xtarget = 0.0;
                    close_current_game();
                    if (open_game(ent->path)) printf("!!!FUCK!!!\n");

                }
            }
            else ent->xtarget = 0.0;

            // The rectangles overlap slightly, so only choose one.
            if (over) already_found = 1;
            
        }

        float minrate = 2.0;
        float rate = 0.10 * ((ent->xtarget < ent->xoffset)? 1.0 : 1.5);
        ent->xoffset = approach(ent->xtarget, minrate, rate, ent->xoffset);
    }


    /** Draw navigation header **/

    draw_hborder(0, border_top_1, top);
    static float bc_pos = 0.0;
    static float bc_offset = 0.0;
    
    int width = 0;

    for (struct breadcrumb *b = browser_breadcrumbs; b != NULL; b = b->next) {
        int bx = (int)(bc_pos + 0.5) + width;
        if (!b->label) b->label = sans_label(color00, 24, b->name);
        if (!b->shadow) b->shadow = sans_label(0x000000, 24, b->name);
        if (!b->highlight) b->highlight = sans_label(color10, 24, b->name);
        drawimage(b->shadow, bx-2, 22+2, left, baseline);
       SDL_Rect rect = drawimage(b->use_highlight? b->highlight : b->label, bx, 22, left, baseline);
        b->use_highlight = mouseover(input, rect);
        width += b->label->w + 1;
        if (b->use_highlight && b->next && (input->pressed & 1)) select_breadcrumb(b);
    }
    
    float excess = max(0.0, width - window_surface->w);
    if (excess > 0.0) excess += 64.0;
    bc_pos = approach((window_surface->w - width)/2 + excess * -bc_offset, 1.0, 0.1, bc_pos);
    if (input->my < border_top_1->h) {
        bc_offset = (input->mx / (float)window_surface->w) - 0.5;
    }


//    drawimage(browser_dir_shadow, window_surface->w / 2 - 2, 22 + 2, center, baseline);
//    drawimage(browser_dir_label,  window_surface->w / 2, 22, center, baseline);

}

/*** Input device menu ***/

void run_input_menu (struct inputctx *input)
{
    if (input->released & SDL_BUTTON(3)) menu = run_main_menu;

    static image_t keyboard_label = NULL;
    if (!keyboard_label) keyboard_label = sans_label(0xFFFFFF, 20, "Keyboard");

    static image_t controller_label = NULL;
    if (!controller_label) controller_label = sans_label(0xFFFFFF, 18, "Controller");

    static image_t num_labels[4] = { NULL, NULL, NULL, NULL };
    if (!num_labels[0]) {
        unsigned size = 20, color = 0xFFFFFF;
        num_labels[0] = sans_label(color, size, "1");
        num_labels[1] = sans_label(color, size, "2");
        num_labels[2] = sans_label(color, size, "3");
        num_labels[3] = sans_label(color, size, "4");
    }

    dim_y_target = vid_height;

    int controller_x = 410;
    drawimage(controller_label, controller_x + 2*47, 3, center, top);
    
    for (int device = 0; device < (1 + numsticks); device++)
    {
        int y = 50 + device * 51;
        int yt = y + 22;
        image_t title = keyboard_label;
        image_t maker = NULL; //description = NULL;

        if (device>0)
        {
            if (!joystick[device-1].title) {
                char buf[256];
                sprintf(buf, "Joystick %i", device);
                joystick[device-1].title = sans_label(0xFFFFFF, 20, buf);
                joystick[device-1].maker = sans_label(0xAAAAAA, 12, SDL_JoystickName(device-1));
            }

            title = joystick[device-1].title;
            maker = joystick[device-1].maker;
        } else yt += 8;

        drawimage(device_bar_bg, 2, y, left, top);
        drawimage(title, 15, yt, left, baseline);
        drawimage(maker, 15, yt+17, left, baseline);

        /* Draw checkmark */
        int next_mapping = -1;
        for (int joypad = 0; joypad < 4; joypad++)
        {
            int x = controller_x + joypad*47;
            int mapped = 0;
            if ((device == 0) && (joypad == cfg_keyboard_controller)) mapped = 1;
            else if ((device>0) && (cfg_jsmap[joypad] == (device-1))) mapped = 1;            
            if (mapped) drawimage(checkmark, x + 43/2, y + 51/2, center, center);
            if (mouseover(input, (SDL_Rect){x, y+2, 43, 47}) && (input->released & SDL_BUTTON(1))) next_mapping = joypad;
        }
        
        if (next_mapping != -1) {
            if (!device) cfg_keyboard_controller = next_mapping;
            else if (cfg_jsmap[next_mapping] == (device-1)) cfg_jsmap[next_mapping] = -1;
            else {
                int cur = cfg_jsmap[next_mapping];
                for (int i=0; i<4; i++) if (cfg_jsmap[i] == device-1) cfg_jsmap[i] = cur;
                cfg_jsmap[next_mapping] = device-1;
            }
        }
    }
    
    /* Draw port columns */
    for (int i=0; i<4; i++)
    {
        int y = 20;
        int x = controller_x + i*47;
        drawimage(rcol_top, x, y, left, top);
        drawimage(num_labels[i], x+21, y+24, center, baseline);
        y += rcol_top->h;
        for (int device = 0; device < (1 + numsticks); device++) {
            drawimage(rcol_middle, x, y, left, top);
            y += rcol_middle->h;
        }
        drawimage(rcol_bottom, x, y, left, top);
    }

}


