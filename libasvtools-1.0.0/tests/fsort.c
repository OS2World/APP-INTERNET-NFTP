#include <stdlib.h>
#include <unistd.h>

#include <asvtools.h>

static int cmp_int (const void *e1, const void *e2)
{
    int *n1, *n2;

    n1 = (int *)e1;
    n2 = (int *)e2;

    return (*n1) - (*n2);
}

int main (int argc, char *argv[])
{
    int    i, n, m, k, n1, *num, *num_s, n_s;
    int    fh_in, fh_out;
    double t1;
    char   *source;

    if (argc != 3)
        error1 ("usage: fsort n m\n"
                "n - number of elements to generate and sort,\n"
                "m - amount of memory (in bytes) to use for memory sort\n");
    n = atoi (argv[1]);
    m = atoi (argv[2]);

    /* produce a file with random integers */
    if (n != 0)
    {
        source = "random.numbers";
        warning1 ("making %d integers...\n", n);
        fh_in = xcreate (source);
        for (i=0; i<n; i++)
        {
            k = random () % n;
            xwrite (fh_in, &k, sizeof(int));
        }
        close (fh_in);
    }
    else
    {
        source = argv[1];
        fh_in = xopen (source);
        n = file_length (fh_in) / sizeof(int);
        close (fh_in);
        warning1 ("file '%s' contains %d integers\n", argv[1], n);
    }

    warning1 ("sorting in %d bytes\n", m);

    fh_in  = xopen   (source);
    fh_out = xcreate ("random.numbers.sorted");
    t1 = clock1 ();
    fsort (fh_in, fh_out, n, sizeof(int), cmp_int, qsort, m, "temp");
    warning1 ("%.3f sec\n", clock1()-t1);
    close (fh_in);
    close (fh_out);

   /* fh_in = xopen ("random.numbers.sorted"); */
   /* for (i=0; i<n; i++) */
   /* { */
   /*    xread (fh_in, &k, sizeof(int)); */
   /*    printf ("%10d\n", k); */
   /* } */
   /* close (fh_in); */

    fh_in  = xopen ("random.numbers.sorted");
    fh_out = xcreate ("random.numbers.sorted.uniq");
    n1 = funiq (fh_in, fh_out, n, sizeof(int), cmp_int);
    close (fh_in);
    close (fh_out);

   /* printf ("-------- %d unique\n", n1); */
   /* fh_in = xopen ("random.numbers.sorted.uniq"); */
   /* for (i=0; i<n1; i++) */
   /* { */
   /*    xread (fh_in, &k, sizeof(int)); */
   /*    printf ("%10d\n", k); */
   /* } */
   /* close (fh_in); */

    printf ("checking...\n");
    n1 = load_file ("random.numbers", (char **)&num);
    n1 /= sizeof (int);
    if (n1 != n) error1 ("inconsistency!\n");
    qsort (num, n, sizeof(int), cmp_int);
    n_s = load_file ("random.numbers.sorted", (char **)&num_s);
    n_s /= sizeof (int);
    if (n_s != n) error1 ("sorted arrays are different (%d != %d)", n_s, n);
    for (i=0; i<n; i++)
        if (num_s[i] != num[i]) error1 ("element %d: %d != %d\n",
                                        i, num_s[i], num[i]);

    return 0;
}

