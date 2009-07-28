#ifndef NESEMU_UTILITY_H
#define NESEMU_UTILITY_H

byte *load_binary_file (char *filename, size_t *size_out);

int load_binary_data (char *filename, void *dest, size_t size);
int save_binary_data (char *filename, void *data, size_t size);
int probe_file (char *path);
int probe_regular_file (char *path);
time_t file_write_date (char *path);

#endif
