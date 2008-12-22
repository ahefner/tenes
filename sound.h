/* external interfaces to sound system */

/* A note on its behavior...
 * Calling snd_update and snd_shutdown should not cause an error even if snd_init was not called (user disabled sound) or if snd_init failed (no audio hardware)
 * snd_update should be called as frequently as possible (twice per frame at least), as it is responsible for checking if the audio buffer needs to be filled (and filling it)
 */

extern int sound_waiting; /* does the buffer need to be filled? */
extern int sound_initialized;
extern int sound_enabled;

int snd_init (void);
/*void snd_update(unsigned char *regs); */
void snd_frameend (void);
void snd_shutdown (void);
void snd_write (unsigned addr, unsigned char value);
