#ifndef NES_UI_H
#define NES_UI_H

#include "sys.h"

struct inputctx {
    int mx, my;                 /* Mouse X/Y */
    int buttons;                /* Mouse button bitmask (level)*/
    int pressed;                /* Mouse pressed bitmask (edge) */
    int released;               /* Mouse released bitmask (edge) */
    int have_pointer;           /* Pointer present? */
};

#ifndef NES_UI_C

#endif



char *asset (char *name);

void dim_to_y (int y);
void dim_background (void);
void run_menu (struct inputctx *input);
int menu_process_key_event (SDL_KeyboardEvent *key);
void open_menu (void);

typedef int alignmode[3];
SDL_Rect drawimage (image_t image, int x, int y, alignmode align_x, alignmode align_y);

int ensure_freetype (void);
image_t sans_label (Uint32 color, unsigned text_height, const char *string);

float approach (float target, float minrate, float rate, float current);

void run_main_menu (struct inputctx *input);
void run_game_browser (struct inputctx *input);
void run_input_menu (struct inputctx *input);

/** Exports **/

#ifndef NES_UI_C
extern void (*menu) (struct inputctx *);
extern float dim_y_target;
#endif

#endif
