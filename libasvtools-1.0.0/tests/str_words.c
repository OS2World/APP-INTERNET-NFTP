#include <stdlib.h>
#include <asvtools.h>

int main (int argc, char *argv[])
{
    char         **L;
    int          i, n;

    if (argc != 3)
    {
        printf ("need a string to parse and integer to indicate spaces\n");
        return -1;
    }

    n = str_words (argv[1], &L, atoi (argv[2]));
    printf ("found %d words:\n", n);
    for (i=0; i<n; i++)
        printf ("[%s] ", L[i]);
    printf ("\n");

    return 0;
}
