#include <stdio.h>
#include "global.h"
#include "rom.h"


int main (int argc, char **argv)
{
  struct nes_rom rom;
  int i;

  if (argc < 2) {
    printf ("Need a filename! %i\n", argc);
    return 0;
  }

  for (i = 1; i < argc; i++) {
    rom = load_nes_rom (argv[i]);
    fprintf(stderr, "%s: %s\n", (rom.mapper < MAPTABLESIZE) ? MapTable[rom.mapper].name : "unknown", argv[i]);
    free_rom (&rom);
  }

  return 0;
}
