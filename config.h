#ifndef CONFIG_H
#define CONFIG_H

void load_config (void);
void save_config (void);
void cfg_parseargs (int argc,const char **argv);

const char *ensure_config_dir (void);
const char *ensure_save_dir (void);
const char *ensure_state_dir(long long hash);

const char *pref_filename (const char *name);

/* Result freed by caller. */
char *pref_string (const char *name, const char *defaultval);

void save_pref_file (const char *name, const byte *data, size_t size);
void save_pref_string (const char *name, const char *string);

#endif
