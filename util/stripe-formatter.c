/* Quick and dirty hack for converting stripe output to a PNG file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <png.h>

unsigned width = 240;
unsigned count = 0, size = 1;
unsigned *buf;

void dumpit (void) 
{
    FILE *out = stdout;
    int height = count;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp rowptrs = malloc(sizeof(png_bytep) * height);
    int i;

    assert(rowptrs);
    for (i=0; i<height; i++) rowptrs[i] = (buf + width * i);

    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, png_voidp_NULL,
                                       png_error_ptr_NULL, png_error_ptr_NULL);
    if (!png_ptr) {
        fprintf(stderr, "libpng lost\n");
        exit(1);
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        fprintf(stderr, "libpng loses again\n");
        exit(1);
    } 
    
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fprintf(stderr, "my head asplode\n");
        exit(1);
    }

    png_init_io(png_ptr, out);

    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    png_set_rows(png_ptr, info_ptr, rowptrs);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    png_write_end(png_ptr, info_ptr);
                    
    // Pretend I free the PNG structures here, if you like.

    fprintf(stderr, "I drew a picture for you on stdout.\n");
}


int main (int argc, char **argv)
{
    unsigned i;
    buf = malloc(width * sizeof(unsigned));
    assert(buf);

    while (!feof(stdin)) {
        /* Grow buffer if necessary */
        if (count == size) {
            unsigned *tmp = realloc(buf, width * size * 2 * sizeof(unsigned));
            
            if (!tmp) {
                tmp = malloc(width * size * 2 * sizeof(unsigned));
                assert(tmp);
                memcpy(tmp, buf, width * size);
                free(buf);
                buf = tmp;
            } else buf = tmp;

            size *= 2;
        }
        
        /* Read the next line */
        if (fread(buf + count * width, width * sizeof(unsigned), 1, stdin) != 1) break;
        count++;
    }

    fprintf(stderr, "Read %u stripes\n", count);

    /* Fix pixel format and alpha */
    for (i=0; i < width*count; i++) 
        buf[i] = 
            ((buf[i] & 0xFF) << 16) | 
            (buf[i] & 0x00FF00) | 
            ((buf[i] & 0xFF0000) >> 16) | 
            0xFF000000;

    /* Write image */
    dumpit();
    
    return 0;
}
