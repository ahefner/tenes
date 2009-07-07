#ifndef NES_FILESYSTEM_H

#include <string.h>

/* Mount the NES filesystem */
int fs_mount (char *mountpoint);

/* Unmount the NES filesystem */
void fs_unmount (void);

/* Returns nonzero if the NES filesystem is mounted, otherwise returns zero. */
int fs_is_mounted (void);

/* Add a fixed-size chunk of binary data for monitoring in the virtual
   filesystem. A reference to the 'name' pointer is retained. */
void fs_add_chunk (const char *name, void *data, size_t size, int writable);

#endif
