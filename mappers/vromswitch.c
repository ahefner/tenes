
/* mapper 3 - VROM switch */

void mapper3_write(register word Addr,register byte Value)
{
    (void)Addr;
    mapper_select_chr_page(0,Value*2);
    mapper_select_chr_page(1,Value*2+1);
}


struct mapper_methods mapper_VROM = {
    mapper0_init,
    mapper0_shutdown,
    0,
    mapper3_write,
    mapper0_read,
    mapper_ignores_scanline_start,
    mapper_ignores_scanline_end,
    nop_save_state,
    nop_restore_state,
    ignore_write,
    ignore_read
};
