#include <stdlib.h>
#include <stdio.h>

#include <asvtools.h>

int main (int argc, char *argv[])
{
    unsigned char   *p;
    unsigned        table[256], to, from;
    long            i, lsrc, ldst;
    FILE            *f;

    // check cmdline arguments
    if (argc != 4)
    {
        printf ("3 arguments: translation-table source destination\n");
        return 1;
    }

    // read conversion table
    for (i=0; i<256; i++) table[i] = i;

    f = fopen (argv[1], "r");
    if (f == NULL) return 2;
    while (fscanf (f, "%u %u", &from, &to) != EOF)
    {
        table[from] = to;
    }
    fclose (f);

    // read source
    lsrc = load_file (argv[2], (char **) &p);
    if (lsrc <= 0)
    {
        printf ("error reading file %s\n", argv[2]);
        return 3;
    }
    printf ("read %ld characters from %s\n", lsrc, argv[2]);

    // do conversion
    for (i=0; i<lsrc; i++) p[i] = table[p[i]];

    // write result
    ldst = dump_file (argv[3], p, lsrc);
    if (ldst != lsrc)
    {
        printf ("error writing file %s\n", argv[3]);
        return 4;
    }

    // the end
    free (p);
    return 0;
}

