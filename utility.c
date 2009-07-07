#include "global.h"
#include "utility.h"



int load_binary_file (char *filename, byte *dest, size_t size)
{
    FILE *in = fopen(filename, "rb");
    if (!in) return 0;
    else {
        int tmp = fread(dest, size, 1, in);
        fclose(in);
        return tmp==1;
    }
}

int save_binary_file (char *filename, byte *data, size_t size)
{
    FILE *out = fopen(filename, "wb");
    if (!out) return 0;
    else {
        int tmp = fwrite(data, size, 1, out);
        fclose(out);
        return tmp==1;
    }
}


