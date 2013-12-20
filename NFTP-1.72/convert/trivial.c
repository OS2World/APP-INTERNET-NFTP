#include "../../include/asvtools.h"

int main (int argc, char *argv[])
{
    char table[256];
    int i;

    for (i=0; i<256; i++)
        table[i] = i;

    dump_file ("trivial", table, 256);
}
