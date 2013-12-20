#include <asvtools.h>

void test_interpolate_l (int argc, char *argv[]);

int main (int argc, char *argv[])
{
    argc--;
    argv++;
    test_interpolate_l (argc, argv);

    return 0;
}

void test_interpolate_l (int argc, char *argv[])
{
    int i, n1, n2;
    double *x1, *y1, *x2, *y2;

    if (argc < 2) error1 ("usage: file1:x1:y1 file2:x2\n");

    n1 = read_two_columns (argv[0], &x1, &y1);
    if (n1 <= 0) error1 ("failed to load %s\n", argv[0]);

    n2 = read_one_column (argv[1], &x2);
    if (n2 <= 0) error1 ("failed to load %s\n", argv[1]);

    y2 = xmalloc (n2 * sizeof (double));

    interpolate_l (x1, y1, n1, x2, y2, n2);

    for (i=0; i<n2; i++)
    {
        printf ("%e14.16 %e24.16\n", x2[i], y2[i]);
    }
}

