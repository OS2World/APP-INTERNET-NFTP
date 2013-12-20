#include <stdlib.h>
#include <stdio.h>

#include <asvtools.h>

int main (int argc, char *argv[])
{
    unsigned char   *p, F[256], T[256];
    unsigned        table[256], to, from, i;
    FILE            *f;

    // check cmdline arguments
    if (argc != 2)
    {
        printf ("1 argument: translation-table\n");
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

    F[0] = "\0";
    T[0] = "\0";
    for (i=0; i<256; i++)
    {
        if (table[i] != i)
        {
            strcat (F, &i);
            strcat (T, &table[i]);
        }
    }
    printf ("from=[%s]\nto=[%s]\n", F, T);

    return 0;
}

