#include "filters.h"
#include "global.h"

/* I've disabled scale2x until I can sort out the licenses. The scaler
 * code is GPL, but M6502 is incompatible with the GPL. My current
 * preference is to drop M6502 so I can GPL this and enable the
 * scaler, or drop them both and license it MIT-style.
 */

/*#include "scale2x.h"*/

#if defined(__GNUC__) && defined(__i386__)
#define scale2x scale2x_8_mmx
#else
#define scale2x scale2x_8_def
#endif


void init_scaling (void)
{
  surface=SDL_CreateRGBSurface(SDL_SWSURFACE,256,241,8,0,0,0,0);
  /* I don't need the second 'post_surface' right now. Probably never will, actually. */
  /*  post_surface=SDL_CreateRGBSurface(SDL_SWSURFACE,512,480,8,0,0,0,0); */
  vid_width=512;
  vid_height=480;  
}



/*
void rescale_filtered (SDL_Surface *dest_s)
{
  unsigned char *src1=surface->pixels;
  unsigned char *src2=src1+surface->pitch;
  unsigned char *src3=src2+surface->pitch;
  unsigned char *dest1=dest_s->pixels;
  unsigned char *dest2=dest1+dest_s->pitch;
  unsigned j, height=surface->h-1;
  unsigned ds=surface->pitch;
  unsigned dd=2*dest_s->pitch;

  for (j=0; j<height; j++)
    {      
      scale2x(dest1,dest2,src1,src2,src3,surface->w);
      dest1+=dd;
      dest2+=dd;
      src1+=ds;
      src2+=ds;
      src3+=ds;
    }  
}*/

void rescale_2x (SDL_Surface *dest_s)
{
  unsigned char *src=surface->pixels;
  unsigned char *dest1=dest_s->pixels;
  unsigned char *dest2=dest1+dest_s->pitch;
  unsigned i,j,w=surface->w, height=surface->h-1;
  unsigned ds=surface->pitch-surface->w;
  unsigned dd=2*dest_s->pitch-dest_s->w;

  for (j=0; j<height; j++)
    {
      for (i=0; i<w; i++)
	{
	  *dest1++=*src;   *dest1++=*src;
	  *dest2++=*src;   *dest2++=*src++;
	}
      dest1+=dd;
      dest2+=dd;
      src+=ds;
    }
}
	
  
  
