#include <asvtools.h>

int main (int argc, char *argv[])
{
    char *p;

    if (argc != 2) error1 ("usage: %s <string>\n", argv[0]);

    printf ("input=%s\n", argv[1]);
    p = xlf_escape (argv[1]);
    printf ("escaped=%s\n", p);
    p = xlf_unescape (p);
    printf ("recovered=%s\n", p);

    return 0;
}
