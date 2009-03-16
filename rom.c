
/* nes rom loader*/
#include "sys.h"
#include "rom.h"
#include "nes.h"
#include "mapper_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>


/* Fantastically expensive to compute, and completely ad-hoc. The best of both worlds. */
static inline unsigned long long hash_add_byte (unsigned long long sum, byte x)
{
    return sum ^ (((sum << 8) + x) % 108086391056891903ll);
}

void hash_bytes (unsigned long long *sum, byte *bytes, unsigned num_bytes)
{
    while (num_bytes--) *sum = hash_add_byte(*sum, *bytes++);
}

unsigned long long rom_hash (unsigned long long file_size, struct nes_rom *rom)
{
    unsigned long long hash = file_size;
    hash_bytes(&hash, rom->header, 16);
    hash_bytes(&hash, rom->prg, rom->prg_size);
    hash_bytes(&hash, rom->chr, rom->chr_size);
    return hash;
}

struct nes_rom load_nes_rom (char *filename)
{
  FILE *in;
  struct stat statbuf;
  struct nes_rom rom;

  in = fopen (filename, "rb");
  if (in == NULL) {
    printf("Could not open file.\n");
    exit(1);
  }

  stat(filename, &statbuf);
  printf("%s\n", filename);
  printf("Rom image size is %i bytes.\n", (int)statbuf.st_size);
  fread((void *) rom.header, 16, 1, in);
  if ((rom.header[0] != 'N') || (rom.header[1] != 'E') || (rom.header[2] != 'S')
      || (rom.header[3] != 0x1A)) {
    printf("Invalid header.\n");
    exit (1);
  }

  rom.prg_size = rom.header[4] * 1024 * 16;
  rom.chr_size = rom.header[5] * 1024 * 8;
  rom.flags = rom.header[6] & 0x0F;	 /* kludge the mapper # */
  rom.mapper = ((rom.header[6] & 0xF0) >> 4) | (rom.header[7] & 0xF0);

  /* This is a hack for roms with bogus headers */
  if ((rom.header[7] == 'D') && (rom.header[8] == 'i')) {
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
  rom.hash = rom_hash(statbuf.st_size, &rom);
  printf("ROM hash is %llX\n", rom.hash);

  printf("PRG ROM is %i bytes\n", rom.prg_size);
  printf("CHR ROM is %i bytes\n", rom.chr_size);  
  printf("Mapper is %i (%s)\n", rom.mapper, rom.mapper_info? rom.mapper_info->name : "corrupt rom header or unknown mapper");
  printf("%s mirroring    %s    %s    %s\n", (rom.flags & 1) ? "Vertical" : "Horizontal", (rom.flags & 2) ? "SRAM" : "", (rom.flags & 4) ? "Trainer" : "", (rom.flags & 8) ? "4-Screen VRAM" : "");
  printf ("\n");

  rom.mirror_mode = rom.flags & 1 ? MIRROR_VERT : MIRROR_HORIZ;
  if (rom.flags & 0x80) rom.mirror_mode = MIRROR_NONE;
  rom.onescreen_page = 0;

  fclose(in);

  return rom;
}

void save_sram (byte save[0x2000], struct nes_rom *rom, int verbose)
{
    char name[PATH_MAX], tmpname[PATH_MAX];

    if (rom->flags & 2) {
        FILE *out;
        strncpy(name, sram_filename(rom), sizeof(name));
        snprintf(tmpname, sizeof(tmpname), "%s-tmp", name);
        unlink(tmpname);
        
        out = fopen(tmpname, "wb");
        if (!out) printf("Warning: Unable to create save file!\n");
        else {
            int n = fwrite(save, 0x2000, 1, out);
            if (fclose(out)) {
                perror("fclose");
                n = 0;
            }

            if (n) {
                if (rename(tmpname, name)) {
                    perror("rename");
                    n = 0;
                }
            }

            if (n && verbose) printf("Saved game to %s\n", sram_filename(rom));
            else if (!n) printf("Error writing save ram.\n");
            
        }
    }
}

void free_rom (struct nes_rom *rom)
{
  free ((void *) rom->prg);
  if (rom->chr != NULL)
    free ((void *) rom->chr);
}
