#include <asvtools.h>

int main (int argc, char *argv[])
{
    if (argc != 3) error1 ("usage: %s pattern teststring\n", argv[0]);

    if (fnmatch1 (argv[1], argv[2], 0) == 0)
        printf ("%s matches %s\n", argv[2], argv[1]);
    else
        printf ("%s does not match %s\n", argv[2], argv[1]);

    return 0;
}
