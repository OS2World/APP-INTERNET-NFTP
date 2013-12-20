#include <asvtools.h>

int main (int argc, char *argv[])
{
    char *p, *n;

    if (argc != 2) 
    {
        printf ("need a pathname to test");
        return -1;
    }
    str_pathdcmp (argv[1], &p, &n);
    printf ("%s: path=[%s], name = [%s]\n", argv[1], p, n);
    return 0;
}
