#include <asvtools.h>

int main (int argc, char *argv[])
{
    char   **str;
    int    n, i;

    if (argc != 2) error1 ("usage: load_textfile filename\n");

    n = load_textfile (argv[1], &str);
    if (n <= 0) error1 ("cannot load %s\n", argv[1]);
    warning1 ("loaded %d lines from %s\n", n, argv[1]);

    for (i=0; i<n; i++)
        printf ("%4d. [%s]\n", i, str[i]);
    return 0;
}
