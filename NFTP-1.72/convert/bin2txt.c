#include <stdio.h>
#include <stdlib.h>

#include "../../include/asvtools.h"

int main (int argc, char *argv[])
{
    char  *pcnv;
    long  lcnv;
    unsigned   i;
    // check cmdline arguments
    if (argc != 2) return 1;

    // read conversion table
    lcnv = load_file (argv[1], &pcnv);
    if (lcnv != 256) return 2;

    for (i=0; i<256; i++)
        if ((unsigned char)pcnv[i] != i)
            printf ("%d %d\n", i, (unsigned char)pcnv[i]);

    // the end
    free (pcnv);
    return 0;
}

