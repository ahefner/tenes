#ifndef CONFIG_H
#define CONFIG_H

void load_config (void);
void save_config (void);
void cfg_parseargs (int argc,char **argv);

char *ensure_config_dir (void);
char *ensure_save_dir (void);
char *ensure_state_dir(long long hash);

char *pref_filename (char *name);
char *pref_string (char *name, char *defaultval);

void save_pref_file (char *name, byte *data, size_t size);
void save_pref_string (char *name, char *string);

#endif
