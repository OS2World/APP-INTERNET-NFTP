#include <asvtools.h>

#include <string.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    char    *p, *p1, *from=NULL, *to=NULL;
    int     i, n, N, ln;

    N = 0;
    if (argc == 5)
    {
        N = atoi (argv[4]);
        from = argv[2];
        to = argv[3];
    }
    else if (argc == 2)
    {
        from = ". .";
        to = ".";
    }
    else if (argc == 4)
    {
        from = argv[2];
        to = argv[3];
    }
    else
    {
        error1 ("Usage: %s <string> <from> <to> [repetitions]\n", argv[0]);
    }

    if (strcmp (argv[1], "-") == 0)
        load_stdin (&p);
    else
        p = strdup (argv[1]);
    ln = strlen (p) + 1;
    p1 = xmalloc (ln);

    if (N == 0)
    {
        memcpy (p1, p, ln);
        n = str_replace (p1, from, to, TRUE);
        printf ("replaced %d occurences\n", n);
        printf ("result = [%s]\n", p1);

        memcpy (p1, p, ln);
        remove_dots (p1);
        printf ("result = [%s]\n", p1);
    }
    else
    {
        double  t1;

        t1 = clock1 ();
        for (i=0; i<N; i++)
        {
            memcpy (p1, p, ln);
            str_replace (p1, from, to, TRUE);
        }
        t1 = clock1 () - t1;
        printf ("%.3f sec, %.3f msec/replace\n", t1, t1*1000.0/N);

        t1 = clock1 ();
        for (i=0; i<N; i++)
        {
            memcpy (p1, p, ln);
            remove_dots (p1);
        }
        t1 = clock1 () - t1;
        printf ("%.3f sec, %.3f msec/remove_dots\n", t1, t1*1000.0/N);
    }

    return 0;
}
