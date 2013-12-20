#include <asvtools.h>

int main (int argc, char *argv[])
{
    char         *s;
    long         l;

    if (argc != 2)
    {
        printf ("need a filename to load");
        return -1;
    }

    l = load_file (argv[1], &s);
    printf ("length: %lu\n", l);

    str_parseindex (s);
    return 0;
}
