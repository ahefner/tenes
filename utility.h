#ifndef NESEMU_UTILITY_H
#define NESEMU_UTILITY_H

int load_binary_file (char *filename, byte *dest, size_t size);
int save_binary_file (char *filename, byte *data, size_t size);

#endif
