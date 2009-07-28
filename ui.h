#ifndef NES_UI_H
#define NES_UI_H

#ifndef NES_UI_C
extern int menu;
extern float dim_y_target;
#endif


struct inputctx {
    int mx, my;                 /* Mouse X/Y */
    int buttons;                /* Mouse button bitmask (level)*/
    int pressed;                /* Mouse pressed bitmask (edge) */
    int released;               /* Mouse released bitmask (edge) */
    int have_pointer;           /* Pointer present? */
};

#ifndef NES_UI_C

#endif


// Stupid image wrapper, because you really need alignment offsets
// attached to the image to have a nice interface. Also helps with
// memory management.

struct myimage {
    int w, h, x_origin, y_origin;
    SDL_Surface *_sdl;
    void *freeptr;
};
typedef struct myimage *image_t;

void image_free (image_t image);

char *asset (char *name);

void dim_to_y (int y);
void dim_background (void);
void run_menu (struct inputctx *input);
int menu_process_key_event (SDL_KeyboardEvent *key);
void open_menu (void);

typedef int alignmode[3];
SDL_Rect drawimage (image_t image, int x, int y, alignmode align_x, alignmode align_y);

int ensure_freetype (void);
image_t sans_label (Uint32 color, unsigned text_height, char *string);
#endif
