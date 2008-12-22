#include <stdio.h>
#include <stdlib.h>
#include "M6502.h"

byte mem[0x10000];


int main (int argc, char **argv)
{
  FILE *in;
  long pc;
  long limit;
  size_t size;
  byte *base;
  

  if (argc!=3) {
    printf("Usage: dis6502 ORG INFILE\n(ORG is in hex)\n");
    return 0;
  }

  in=fopen(argv[2],"rb");
  if (!in) {
    fprintf(stderr,"Could not open \"%s\"\n",argv[2]);
    return 1;
  }

  pc=strtol(argv[1],NULL,16);
  fprintf(stderr,"Disassembly starting at $%04X\n",(unsigned int)pc);
  
  size=fread(mem,1,0x10000,in);
  limit=pc+size;
  base=mem-pc;
  fclose(in);

  fprintf(stderr,"Disassembling $%04X bytes (%i kilobytes)\n",size,size>>10);

  while (pc<limit) {
    char txt[256]="";
    pc+=DAsm(txt,base+pc,pc);
    printf("%04X %s\n",(unsigned)pc,txt);
  }  

  return 0;  
}
