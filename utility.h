#ifndef NESEMU_UTILITY_H
#define NESEMU_UTILITY_H

byte *load_binary_file (const char *filename, size_t *size_out);

int load_binary_data (const char *filename, void *dest, size_t size);
int save_binary_data (const char *filename, const void *data, size_t size);
int probe_file (const char *path);
int probe_regular_file (const char *path);
time_t file_write_date (const char *path);

char *make_absolute_filename (const const char *filename);

static inline float clampf (float min, float max, float value)
{
    if (value < min) return min;
    else if (value > max) return max;
    else return value;
}

static inline int clampi (int min, int max, int value)
{
    if (value < min) return min;
    else if (value > max) return max;
    else return value;
}

const char *format_binary (byte x);

#endif
