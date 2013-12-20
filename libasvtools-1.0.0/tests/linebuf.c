#include <asvtools.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    char *fname, *p;
    int  rc, msize;
    linebuf_t lb;
    FILE *fp;

    if (argc != 3) error1 ("usage: filename memsize\n");

    fname = argv[1];
    msize = atoi (argv[2]);

    warning1 ("file %s, memsize %u\n", fname, msize);

    fp = fopen (fname, "r");
    if (fp == NULL) error1 ("can't open %s\n", fname);

    rc = linebuf_init (&lb, fp, msize);
    if (rc) error1 ("linebuf_init() failed with rc=%d\n", rc);

    while ((rc = linebuf_nextline (&lb, &p)) == 0)
    {
        puts (p);
    }
    if (rc != 1) error1 ("linebuf_nextline() failed with rc=%d\n", rc);

    rc = linebuf_close (&lb);
    if (rc) error1 ("linebuf_close() failed with rc=%d\n", rc);

    return 0;
}
