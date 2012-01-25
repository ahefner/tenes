#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "global.h"
#include "utility.h"

byte *load_binary_file (char *filename, size_t *len_out)
{
    FILE *in = fopen(filename, "rb");
    if (!in) return 0;
    else {
        fseek(in, 0, SEEK_END);
        long size = ftell(in);

        if (size < 1)
        {
            fclose(in);
            return NULL;
        }

        byte *dest = calloc(1, size+1);

        if (!dest)
        {
            fclose(in);
            return NULL;
        }

        rewind(in);
        int tmp = fread(dest, size, 1, in);
        fclose(in);

        if (tmp == 1) {
            if (len_out) *len_out = size;
            return dest;
        } else {
            free(dest);
            return NULL;
        }
    }
}

int load_binary_data (char *filename, void *dest, size_t size)
{
    FILE *in = fopen(filename, "rb");
    if (!in) return 0;
    else {
        int tmp = fread(dest, size, 1, in);
        fclose(in);
        return tmp==1;
    }
}

int save_binary_data (char *filename, void *data, size_t size)
{
    FILE *out = fopen(filename, "wb");
    if (!out) return 0;
    else {
        int tmp = fwrite(data, size, 1, out);
        fclose(out);
        return tmp==1;
    }
}


int probe_file (char *path)
{
    struct stat st;
    return !stat(path, &st);
}

int probe_regular_file (char *path)
{
    struct stat st;
    if (stat(path, &st)) return 0;
    else return S_ISREG(st.st_mode);
}

time_t file_write_date (char *path)
{
    struct stat st;
    if (!stat(path, &st)) return st.st_mtime;
    else return (time_t)0;
}

char *make_absolute_filename (char *filename)
{
#ifdef _WIN32
    printf("FIXME make absolute filename\n");
    return strdup(filename);
#else
    if (filename[0] == '/') return strdup(filename);
    else {
        char buf[1024];
        // Non standard, but documented to work on OS X and glibc.
        char *cwd = getcwd(NULL,0);
        snprintf(buf, sizeof(buf), "%s/%s", cwd, filename);
        free(cwd);
        return strdup(buf);
    }
#endif
}
