#include <asvtools.h>

int main (int argc, char *argv[])
{
    int64_t n;

    for (n=1; n!=0; n*=2)
        printf ("%s = [%s]\n", printf64(n), pretty64(n));
    printf ("%s = [%s]\n", printf64(-1LL), pretty64(-1LL));

    return 0;
}

