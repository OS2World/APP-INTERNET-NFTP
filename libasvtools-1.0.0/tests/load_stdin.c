#include <asvtools.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
    char   *str;
    int    n;

    n = load_stdin (&str);
    if (n <= 0) error1 ("cannot load from stdin\n");
    warning1 ("loaded %d bytes from stdin\n", n);

    write (fileno(stdout), str, n);
    return 0;
}
