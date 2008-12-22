
/* mapper 3 - VROM switch */

int mapper3_init(void)
{
   nes.mapper_data=(malloc(0x8000));
   memcpy(nes.mapper_data,(void *)nes.rom.prg,0x8000);
   memcpy((void *)nes.ppu.vram,(void *)nes.rom.chr,0x2000);
   return 1;
}

void mapper3_shutdown(void)
{
   free(nes.mapper_data);
}

void mapper3_write(register word Addr,register byte Value)
{   
   unsigned tmp=Value*0x2000;
   if(tmp>(nes.rom.chr_size-0x2000)) tmp%=nes.rom.chr_size;
   memcpy((void *)nes.ppu.vram,(void *)(nes.rom.chr+tmp),0x2000);
}


struct mapper_functions mapper_VROM = {
   mapper3_init,
     mapper3_shutdown,
     mapper3_write,
     mapper0_read
};
