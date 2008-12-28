
/* mapper 2 - Konami */

int mapper2_numpages;

int mapper2_init(void)
{
   mapper2_numpages = nes.rom.prg_size / 0x4000;
   nes.mapper_data=malloc(0x8000);
   memcpy(nes.mapper_data,(void *)nes.rom.prg,0x4000);
   memcpy((void *)((unsigned char *)nes.mapper_data+0x4000),(void *)(nes.rom.prg+(mapper2_numpages-1)*0x4000),0x4000);

   return 1;
}

void mapper2_shutdown(void)
{
   free(nes.mapper_data);
}

void mapper2_write(register word Addr,register byte Value)
{
   unsigned tmp=Value;
   if(Value>=mapper2_numpages) tmp%=mapper2_numpages;
   tmp*=0x4000;
   memcpy(nes.mapper_data,(void *)(nes.rom.prg+tmp),0x4000);
}

struct mapper_methods mapper_konami = {
   mapper2_init,
   mapper2_shutdown,
   mapper2_write,
   mapper0_read,
   mapper0_scanline
};
   
