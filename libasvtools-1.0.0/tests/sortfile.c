#include <asvtools.h>

#include <stdlib.h>

int main (int argc, char *argv[])
{
    char *fn1, *fn2;
    int  msize, rc;
    double t1;

    if (argc != 4) error1 ("usage: sortfile filename1 filename2 memory\n");

    fn1 = argv[1];
    fn2 = argv[2];
    msize = atoi (argv[3]);

    warning1 ("sorting %s -> %s using %d bytes\n", fn1, fn2, msize);

    t1 = clock1 ();
    rc = sortfile (fn1, fn2, cmp_str, msize, ".");
    t1 = clock1 () - t1;

    warning1 ("finished in %.3f sec; rc = %d\n", t1, rc);

    return 0;
}

