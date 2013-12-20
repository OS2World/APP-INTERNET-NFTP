#include <stdlib.h>
#include <unistd.h>

#include <asvtools.h>

int array[] =
{-10, 0, 1, 5, 6, 7, 12, 13, 14, 14, 14, 14, 15, 17, 28, 29, 30};
/* {0, 1}; */
int n = sizeof(array)/sizeof(int);

int main (int argc, char *argv[])
{
    int    i, rc, left, right, num;

    printf ("Array: ");
    for (i=0; i<n; i++) printf ("%d ", array[i]);
    printf ("\n");

    for (num=-12; num<33; num++)
    {
        rc = bracket (&num, array, n, sizeof(int), cmp_integers, &left, &right);
        printf ("lookup %3d: rc=%2d, ", num, rc);
        if (rc >= 0)
        {
            printf ("%d %d: ", left, right);
            for (i=0; i<n; i++)
                if (i == left && i == right) printf ("[%d] ", array[i]);
                else if (i == left)          printf ("[%d ", array[i]);
                else if (i == right)         printf ("%d] ", array[i]);
                else                         printf ("%d ", array[i]);
        }
        printf ("\n");
    }

    return 0;
}

