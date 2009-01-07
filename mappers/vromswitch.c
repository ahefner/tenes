
/* mapper 3 - VROM switch */

void mapper3_write(register word Addr,register byte Value)
{   
   unsigned tmp = Value*0x2000;
   if (tmp > (nes.rom.chr_size-0x2000)) tmp %= nes.rom.chr_size;
   memcpy((void *)nes.ppu.vram,(void *)(nes.rom.chr+tmp),0x2000);
}


struct mapper_methods mapper_VROM = {
   mapper0_init,
   mapper0_shutdown,
   mapper3_write,
   mapper0_read,
   mapper0_scanline,
   nop_save_state,
   nop_restore_state   
};
