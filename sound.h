
/* External interface to sound system */

int snd_init (void);
void snd_shutdown (void);
void snd_write (unsigned addr, unsigned char value);
void snd_catchup (void);
unsigned char snd_read_status_reg (void);



