
/* mapper 3 - VROM switch */

int mapper3_init(void)
{
    mapper0_init();
    return 1;
}

void mapper3_shutdown(void)
{
   free(nes.mapper_data);
}

void mapper3_write(register word Addr,register byte Value)
{   
   unsigned tmp = Value*0x2000;
   //printf("mapper 3: selected bank %i\n", Value);
   if (tmp > (nes.rom.chr_size-0x2000)) tmp %= nes.rom.chr_size;
   memcpy((void *)nes.ppu.vram,(void *)(nes.rom.chr+tmp),0x2000);
   vid_tilecache_dirty = 1;
}


struct mapper_methods mapper_VROM = {
   mapper3_init,
   mapper3_shutdown,
   mapper3_write,
   mapper0_read,
   mapper0_scanline
};
