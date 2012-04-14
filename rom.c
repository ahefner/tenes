
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
#include <unistd.h>


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

/* FIXME: This would be more useful if you could distinguish
 * failure. Aborting when load fails is no longer acceptable now that
 * we have a fancy GUI and support changing ROMs at runtime. */

void print_nsf_header_info (struct nsf_header *h)
{
    printf("NSF Header:\n");
    printf("  Name: \"%s\"\n", h->name);
    printf("  Artist: \"%s\"\n", h->artist);
    printf("  Copyright: \"%s\"\n", h->copyright);
    printf("  Version: %02Xh\n", h->version);
    printf("  Total songs: %i\n", h->total_songs);
    printf("  Starting song: %i\n", h->starting_song);
    printf("  Load $%04X / Init $%04X / Play $%04X\n",
           h->load_addr, h->init_addr, h->play_addr);

    printf("  Mode: ");
    switch (h->pal_mode & 3) {
    case 0:
        printf("NTSC\n");
        printf("  Speed: %i\n", h->speed_ntsc);
        break;
    case 1:
        printf("PAL\n");
        printf("  Speed: %i\n", h->speed_pal);
        break;
    case 2:
    case 3:
        printf("Dual NTSC/PAL\n");
        printf("  NTSC Speed: %i\n", h->speed_ntsc);
        printf("  PAL Speed: %i\n", h->speed_pal);
        break;
    }

    printf("  Chipflags: %02X\n", h->chipflags);
    printf("  Bank setup: ");
    for (int i=0; i<8; i++) printf("$%02X ", h->bankswitch[i]);
    printf("\n");
}

int load_nsf (struct nes_rom *rom, FILE *in, int filesize)
{
    static struct nsf_header header;
    assert(sizeof(header) == 0x80);

    fseek(in, 0, SEEK_SET);
    if (1 != fread(&header, 0x80, 1, in)) {
        printf("Incomplete header.\n");
        return 0;
    }

    print_nsf_header_info(&header);

    if (header.total_songs < 1) {
        printf("NSF has no songs!\n");
        return 0;
    }

    rom->prg_size = filesize - 0x80;
    printf("Music program is %i ($%X) bytes\n", rom->prg_size, rom->prg_size);
    rom->prg = malloc(rom->prg_size);
    rom->chr_size = 0;
    rom->chr = NULL;
    rom->mapper_info = get_NSF_minf();
    rom->hw_mirror_mode = MIRROR_HORIZ;
    rom->hw_onescreen_page = 0;
    rom->nsf_header = &header;

    if (!rom->prg) return 0;
    if (1 != fread(rom->prg, rom->prg_size, 1, in)) return 0;

    return 1;
}

int load_nsfe (struct nes_rom *rom, FILE *in, int filesize)
{
    printf("Not implemented.\n");
    return 0;
}

int load_ines (struct nes_rom *rom, FILE *in, int filesize)
{
    rom->prg_size = rom->header[4] * 1024 * 16;
    rom->chr_size = rom->header[5] * 1024 * 8;
    rom->flags = rom->header[6] & 0x0F;	 /* kludge the mapper # */
    rom->mapper = ((rom->header[6] & 0xF0) >> 4) | (rom->header[7] & 0xF0);

    /* This is a hack for roms with bogus headers */
    if ((rom->header[7] == 'D') && (rom->header[8] == 'i')) {
        printf ("This rom appears to have a corrupt header.\n");
        rom->mapper &= 0x0F;
    }

    rom->mapper_info = mapper_find(rom->mapper);

    rom->prg = (byte *)malloc(rom->prg_size);
    if (rom->chr_size) rom->chr = (byte *)malloc(rom->chr_size);
    else rom->chr = NULL;

    if ((rom->prg == NULL) || (rom->chr_size && (rom->chr == NULL))) {
        printf("Cannot allocate memory for rom data.\n");
        return 0;
    }

    fread((void *)rom->prg, rom->prg_size, 1, in);
    fread((void *)rom->chr, rom->chr_size, 1, in);

    printf("PRG ROM is %i bytes\n", rom->prg_size);
    printf("CHR ROM is %i bytes\n", rom->chr_size);
    printf("Mapper is %i (%s)\n",
           rom->mapper,
           rom->mapper_info? rom->mapper_info->name :
                            "corrupt rom header or unknown mapper");

    printf("%s mirroring    %s    %s    %s\n",
           (rom->flags & 1) ? "Vertical" : "Horizontal",
           (rom->flags & 2) ? "SRAM" : "",
           (rom->flags & 4) ? "Trainer" : "",
           (rom->flags & 8) ? "4-Screen VRAM" : "");

    printf ("\n");

    rom->hw_mirror_mode = rom->flags & 1 ? MIRROR_VERT : MIRROR_HORIZ;
    if (rom->flags & 0x80) rom->hw_mirror_mode = MIRROR_NONE;
    rom->hw_onescreen_page = 0;

    return 1;
}

struct nes_rom load_nes_rom (const char *filename)
{
    FILE *in;
    struct stat statbuf;
    struct nes_rom rom;

    memset(&rom, 0, sizeof(rom));

    in = fopen (filename, "rb");
    if (in == NULL) {
        printf("Could not open file.\n");
        exit(1);
    }

    strcpy(rom.title, filename);
    strcpy(rom.filename, filename);

    stat(filename, &statbuf);
    int filesize = (int)statbuf.st_size;
    printf("%s\n", filename);
    printf("Rom image size is %i bytes.\n", filesize);
    fread((void *) rom.header, 16, 1, in);

    byte ines_magic[4] = { 'N', 'E', 'S', 0x1A };
    if (!memcmp(rom.header, ines_magic, 4)) {
        printf("iNES format detected.\n");
        rom.machine_type = NES_NTSC;
        if (load_ines(&rom, in, filesize)) goto success;
    }

    byte nsf_magic[5] = { 'N', 'E', 'S', 'M', 0x1A };
    if (!memcmp(rom.header, nsf_magic, 5)) {
        rom.machine_type = NSF_PLAYER;
        if (load_nsf(&rom, in, filesize)) goto success;
    }

    byte nsfe_magic[5] = { 'N', 'S', 'F', 'E', 0x1A };
    if (!memcmp(rom.header, nsfe_magic, 5)) {
        rom.machine_type = NSF_PLAYER;
        if (load_nsfe(&rom, in, filesize)) goto success;
    }

/* failure: */
    printf("Error loading or unrecognized file format.\n");
    fclose(in);
    exit(1);

success:
    rom.hash = rom_hash(filesize, &rom);
    printf("ROM hash is %llX\n", rom.hash);
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
