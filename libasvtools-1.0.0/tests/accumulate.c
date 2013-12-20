#include <asvtools.h>
#include <stdlib.h>

/* compare integers */
int cmp_integers_n (const void *e1, const void *e2)
{
    int *n1, *n2;

    n1 = (int *) e1;
    n2 = (int *) e2;

    return - *n1 + *n2;
}


int main (int argc, char *argv[])
{
   int    *a, *b;
   int    i, x, n, na, N;

   N = 1000;
   a = malloc (sizeof (int) * N);

   n = 100;
   b = malloc (sizeof (int) * n);

   na = 0;
   for (i=0; i<1000; i++)
   {
       x = random () % 100000;
       a[i] = x;
       accumulate (b, &x, sizeof (int), &na, n, cmp_integers);
   }

   qsort (a, N, sizeof (int), cmp_integers);
   qsort (b, n, sizeof (int), cmp_integers);

   printf ("\n");
   for (i=0; i<n; i++)
       printf ("%d %d\n", a[i], b[i]);

   return 0;
}

