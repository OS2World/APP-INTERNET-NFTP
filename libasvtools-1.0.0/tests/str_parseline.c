#include <asvtools.h>

int main (int argc, char *argv[])
{
    char         **L;
    int          i, n;

    if (argc != 2)
    {
        printf ("need a string to parse");
        return -1;
    }

    n = str_parseline (argv[1], &L, ' ');
    printf ("found %d components:\n", n);
    for (i=0; i<n; i++)
        printf ("[%s] ", L[i]);

    printf ("\njoined: [%s]\n", str_mjoin (L, n, '@'));

    return 0;
}
