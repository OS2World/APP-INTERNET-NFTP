#include <stdlib.h>

#include <asvtools.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static int cmp_int (const void *e1, const void *e2)
{
    int *n1, *n2;

    n1 = (int *)e1;
    n2 = (int *)e2;

    return (*n1) - (*n2);
}

int main (int argc, char *argv[])
{
    int  i, n, n1, *A, alg;
    double t1;

    if (argc > 1)
        n = atoi (argv[1]);
    else
        n = 20;

    if (argc > 2)
        alg = atoi (argv[2]);
    else
        alg = 0;

    A = xmalloc (sizeof(int) * n);
    for (i=0; i<n; i++) A[i] = random () % 1000;

    for (i=0; i<min1(n, 20); i++) printf ("%d ", A[i]);
    printf ("\n");

    t1 = clock1 ();
    switch (alg)
    {
    case 0:
        n1 = psort (A, n, sizeof(int), 10, cmp_int);
        break;

    case 1:
        n1 = psort1 (A, n, sizeof(int), 10, cmp_int);
        break;

    default:
        n1 = 0;
    }
    t1 = clock1() - t1;

    printf ("%d elements were partially sorted in %.3f sec\n", n1, t1);
    for (i=0; i<min1(n, 10); i++) printf ("%d ", A[i]);
    printf ("\n");

    return 0;
}
