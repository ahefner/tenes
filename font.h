#ifndef NESEMU_FONT_H
#define NESEMU_FONT_H

static const int text_ascent = 10;
static const int text_descent = 4;
static const int text_height = 14;
static const int text_width = 8;

static const Uint32 white = 0xFFFFFF;

int draw_string (int x0, int baseline, char *string, unsigned color);

#endif
