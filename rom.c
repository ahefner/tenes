
/* nes rom loader*/

#include "rom.h"
#include "mapper_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct nes_rom load_nes_rom (char *filename)
{
  FILE *in;
  struct stat statbuf;
  byte header[16];
  struct nes_rom rom;

  in = fopen (filename, "rb");
  if (in == NULL) {
    printf("Could not open file.\n");
    exit(1);
  }

  stat(filename, &statbuf);
  printf("%s\n", filename);
  printf("Rom image size is %i bytes.\n", (int)statbuf.st_size);
  fread((void *) header, 16, 1, in);
  if ((header[0] != 'N') || (header[1] != 'E') || (header[2] != 'S')
      || (header[3] != 0x1A)) {
    printf("Invalid header.\n");
    exit (1);
  }

  rom.prg_size = header[4] * 1024 * 16;
  rom.chr_size = header[5] * 1024 * 8;
  rom.flags = header[6] & 0x0F;	 /* kludge the mapper # */
  rom.mapper = ((header[6] & 0xF0) >> 4) | (header[7] & 0xF0);

  /* This is a hack for roms with bogus headers */
  if ((header[7] == 'D') && (header[8] == 'i')) {
    printf ("This rom appears to have a corrupt header.\n");
    rom.mapper &= 0x0F;
  }

  rom.mapper_info = mapper_find(rom.mapper);

  strcpy(rom.title, filename);
  strcpy(rom.filename, filename);

  rom.prg = (byte *)malloc(rom.prg_size);
  rom.chr = (byte *)malloc(rom.chr_size);

  if ((rom.prg == NULL) || (rom.chr == NULL)) {
    printf("Cannot allocate memory for rom data.\n");
    exit (1);
  }

  fread((void *)rom.prg, rom.prg_size, 1, in);
  fread((void *)rom.chr, rom.chr_size, 1, in);

  memset((void *)rom.save, 0, 0x2000);

  printf("PRG ROM is %i bytes\n", rom.prg_size);
  printf("CHR ROM is %i bytes\n", rom.chr_size);  
  printf("Mapper is %i (%s)\n", rom.mapper, rom.mapper_info? rom.mapper_info->name : "corrupt rom header or unknown mapper");
  printf("%s mirroring    %s    %s    %s\n", (rom.flags & 1) ? "Vertical" : "Horizontal", (rom.flags & 2) ? "SRAM" : "", (rom.flags & 4) ? "Trainer" : "", (rom.flags & 8) ? "4-Screen VRAM" : "");
  printf ("\n");

  rom.mirror_mode = rom.flags & 1 ? MIRROR_VERT : MIRROR_HORIZ;
  if (rom.flags & 0x80) rom.mirror_mode = MIRROR_NONE;
  rom.onescreen_page = 0;

  fclose (in);
  return rom;
}

void free_rom (struct nes_rom *rom)
{
  free ((void *) rom->prg);
  if (rom->chr != NULL)
    free ((void *) rom->chr);
}
