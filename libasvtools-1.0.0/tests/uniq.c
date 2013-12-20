#include <stdio.h>

#include <asvtools.h>

int a[] = {0, 0, 0, 1, 2, 3, 3, 3, 4, 5, 6, 7, 7, 7, 7};

int main (int argc, char *argv[])
{
    int i, n, n1;

    n = sizeof(a)/sizeof(a[0]);

    for (i=0; i<n; i++)
        printf ("%d ", a[i]);
    printf ("\n");

    n1 = uniq2 (a, n, sizeof(a[0]), cmp_integers);

    printf ("%d elements remain after uniq() from %d\n", n1, n);
    for (i=0; i<n; i++)
        printf ("%d ", a[i]);
    printf ("\n");

    return 0;
}

