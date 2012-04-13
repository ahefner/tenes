#ifndef USE_FUSE

#include <string.h>

/* Dummy version of vfile creators, so callers don't have to conditionalize. */
void fs_add_chunk (char *name, void *data, size_t size, int writable)
{
}

#else

/*** FUSE made easy. ***/

#define FUSE_USE_VERSION 26
#define __USE_XOPEN
#define __need_timespec
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <fuse.h>

#include "filesystem.h"

static char fs_mountpoint[PATH_MAX];
static struct fuse *fuse;
static struct fuse_chan *ch;
static int fs_mounted = 0;

struct vfile {
    const char *name;
    enum { chunk } type;
    struct vfile *next;
    int writable;

    void *data;
    size_t data_size;
};

struct vfile *vfiles = NULL;

void fs_add_chunk (const char *name, void *data, size_t size, int writable)
{
    struct vfile *vf = malloc(sizeof(struct vfile));
    assert(vf);
    vf->name = name;
    vf->type = chunk;
    vf->writable = (writable!=0);
    vf->data = data;
    vf->data_size = size;
    vf->next = vfiles;
    vfiles = vf;
}

static struct vfile *find_vfile (const char *name)
{
    struct vfile *vf = vfiles;
    if (name[0] == '/') name++;
    while ((vf != NULL) && strcmp(name, vf->name)) vf = vf->next;
    return vf;
}

static int fs_getattr (const char *path, struct stat *stbuf)
{
    struct vfile *vf = find_vfile(path);
    memset(stbuf, 0, sizeof(struct stat));

    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    } else if (vf) {
        stbuf->st_mode = S_IFREG | (vf->writable? 0666 : 0444);
        stbuf->st_nlink = 1;
        stbuf->st_size = vf->data_size;
        return 0;
    } else return -ENOENT;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    struct vfile *vf = vfiles;
    if(strcmp(path, "/") != 0) return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    while (vf) {
        filler(buf, vf->name, NULL, 0);
        vf = vf->next;
    }

    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    struct vfile *vf = find_vfile(path);
    if (!vf) return -ENOENT;
    if ((!vf->writable) && ((fi->flags & 3) != O_RDONLY)) return -EACCES;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    struct vfile *vf = find_vfile(path);
    void *data;
    size_t len;

    if (!vf) return -ENOENT;

    switch (vf->type) {
    case chunk:
        data = vf->data;
        len = vf->data_size;
        break;
    default: assert(0);
    }

    if (offset < len) {
        if (offset + size > len) size = len - offset;
        memcpy(buf, data + offset, size);
    } else size = 0;

    return size;
}

static int fs_truncate(const char *path, off_t offset)
{
    struct vfile *vf = find_vfile(path);
    if (!vf) return -ENOENT;

    switch (vf->type) {
    case chunk:
        if (offset != vf->data_size) return EPERM;
        break;
    default: assert(0);
    }

    return 0;
}

static int fs_write (const char *path, const char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi)
{
    struct vfile *vf = find_vfile(path);
    if (!vf) return -ENOENT;
    if (!vf->writable) return -EPERM;

    /* printf("fs_write: %s offset %X size %X\n", path, (int)offset, (int)size); */

    switch (vf->type) {

    case chunk:
    {
        if (offset >= vf->data_size) return -EFBIG;
        if (size > vf->data_size) return -EFBIG;
        if (offset + size > vf->data_size) return -EFBIG;
        memcpy(vf->data + offset, buf, size);
    } break;

    default: assert(0);
    }

    return size;
}

static struct fuse_operations fs_ops = {
    .getattr	= fs_getattr,
    .readdir	= fs_readdir,
    .open       = fs_open,
    .read	= fs_read,
    .write      = fs_write,
    .truncate   = fs_truncate
};

static void *run_fs_thread (void *arg)
{
    fuse_loop(fuse);
    return NULL;
}

/* Mount the NES filesystem */
int fs_mount (char *mountpoint)
{
    pthread_t thread;
    if (fs_mounted) return 0;

    mkdir(mountpoint, 0700);

    if (strlen(mountpoint) + 1 >= sizeof(fs_mountpoint)) return 0;
    strcpy(fs_mountpoint, mountpoint);

    ch = fuse_mount(mountpoint, NULL);
    if (!ch) {
        printf("fuse_mount failed!\n");
        return 0;
    }

    fuse = fuse_new(ch, NULL, &fs_ops, sizeof(fs_ops), NULL);
    if (!fuse) {
        printf("fuse_new failed!\n");
        fuse_unmount(mountpoint, NULL);
        return 0;
    }

    fuse_set_signal_handlers(fuse_get_session(fuse));
    fs_mounted = 1;
    atexit(fs_unmount);

    if (pthread_create(&thread, NULL, run_fs_thread, NULL)) {
        printf("Error creating FUSE loop thread.\n");
        fs_unmount();
        return 0;
    }

    printf("Mounted NES virtual filesystem at %s\n", fs_mountpoint);

    return 1;
}

/* Unmount the NES filesystem */
void fs_unmount (void)
{
    if (!fs_mounted) return;

    fuse_remove_signal_handlers(fuse_get_session(fuse));
    fuse_unmount(fs_mountpoint, ch);
    fuse_destroy(fuse);
}

/* Returns nonzero if the NES filesystem is mounted, otherwise returns zero. */
int fs_is_mounted (void)
{
    return fs_mounted;
}

#endif
