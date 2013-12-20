#include <asvtools.h>

int main (int argc, char *argv[])
{
    int    rc, n;

    if (argc < 2) error1 ("usage: is_textfile filename\n");

    n = 0;
    while (argv[n++] != NULL)
    {
        rc = is_textfile (argv[n], -1);
        switch (rc)
        {
        case 0:  warning1 ("%s: binary file\n", argv[n]); break;
        case 1:  warning1 ("%s: text file\n", argv[n]); break;
        case -1: warning1 ("%s: error\n", argv[n]);
        }
    }

    return 0;
}
