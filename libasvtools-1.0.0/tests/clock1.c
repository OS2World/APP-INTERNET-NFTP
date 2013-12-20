#include <asvtools.h>
#include <stdlib.h>
#include <math.h>

int main (int argc, char *argv[])
{
    int i, n;
    double t1, t2, t3, t4;

    n = 0;
    if (argc == 2) n = atoi (argv[1]);
    if (n == 0) n = 1000000;

    printf ("doing %d iterations of clock1()... ", n);
    fflush (stdout);
    t1 = clock1 ();
    t2 = t1;
    t4 = 0.;
    for (i=0; i<n; i++)
    {
        t3 = clock1 ();
        t4 += t3 - t2;
        t2 = t3;
    }
    t1 = clock1() - t1;
    printf ("done\n");
    printf ("running time: %.6f sec\n", t1);
    printf ("average time of clock1() call: %.6f microseconds\n", t1/n*1.e6);
    printf ("accumulated time: %.6f sec\n", t4);
    printf ("accumulated error: %.6f microseconds\n", fabs((t4-t1)*1.e6));

    return 0;
}
