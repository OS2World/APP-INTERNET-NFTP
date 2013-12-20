#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#ifndef __MINGW32__
#include <sys/param.h>
#else
#include <process.h>
#endif

#ifndef __MINGW32__
#include <sys/param.h>
#else
#define MAXPATHLEN 260
#endif

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* compare strings */
int cmp_str (const void *e1, const void *e2)
{
    char **s1, **s2;

    s1 = (char **) e1;
    s2 = (char **) e2;

    return strcmp (*s1, *s2);
}

/* compare lengths of strings */
int cmp_strlen (const void *e1, const void *e2)
{
    char **s1, **s2;
    int  c;

    s1 = (char **) e1;
    s2 = (char **) e2;

    c = strlen (*s2) - strlen(*s1);
    if (c) return c;

    return strcmp (*s1, *s2);
}

/* compare integers */
int cmp_integers (const void *e1, const void *e2)
{
    int *n1, *n2;

    n1 = (int *) e1;
    n2 = (int *) e2;

    return *n1 - *n2;
}

/* compare doubles */
int cmp_doubles (const void *e1, const void *e2)
{
    double *d1, *d2;
     
    d1 = (double *) e1;
    d2 = (double *) e2;
             
    if (*d1 > *d2) return 1;
    if (*d1 < *d2) return -1;
    return 0;
}

/* compare integers */
int cmp_absintegers (const void *e1, const void *e2)
{
    int *n1, *n2;

    n1 = (int *) e1;
    n2 = (int *) e2;

    return abs(*n1) - abs(*n2);
}

/* compare unsigned integers */
int cmp_unsigned_integers (const void *e1, const void *e2)
{
    unsigned int *n1, *n2;
    
    n1 = (unsigned int *) e1;
    n2 = (unsigned int *) e2;
            
    if (*n1 > *n2) return 1;
    if (*n1 < *n2) return -1;
    return 0;
}

/* sorts binary file using partial sort and merge (useful for large sorts).
 * Parameters:
 * fh_in, fh_out -- must be preopened handles for input/output files;
 * nmemb, size -- number of elements in input stream and size (in bytes)
 *   of each element, just like in qsort();
 * compar -- function to compare elements, like in qsort();
 * sort1 -- function which will be used for memory-based sort (specify
 *   NULL to use libc's qsort());
 * msize -- max. available memory for sorting, in bytes
 * tmp_path -- directory where temporary files will be written. when
 *   NULL is specified, system default is used (NULL DOES NOT WORK YET)
 */
int fsort_old (int fh_in, int fh_out, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *),
           void (*sort1)(void *base, size_t nmemb, size_t size, int (*compar1)(const void *, const void *)),
           int msize, char *tmp_path)
{
    void   *base;
    int    nw, fh, i, j, nsrc, n_0, n_l, n, n_s, pid, n1;
    double t0;
    struct
    {
        int  fh; /* file handle */
        int  ci; /* current index while merging */
        int  n;  /* number of elements in the file */
        void *e;
    }
    *f;

    /* see if we can do memory-based sort (i.e. specified memory size is
     enough to hold entire data array) */
    if (nmemb*size <= msize)
    {
        /* warning1 ("entire array will be sorted in memory\n"); */
        base = xmalloc (nmemb*size);
        xread (fh_in, base, nmemb*size);
        sort1 (base, nmemb, size, compar);
        xwrite (fh_out, base, nmemb*size);
        free (base);
        return 0;
    }

    pid = getpid ();
    
    /* find out how many entries (n_0) we will be able to sort in one gulp
     i.e., in memory with sort1(), how many steps it will take (ns), and
     what will be the size of the last gulp (n_l) */
    n_0 = msize / size;
    if (n_0 < 2) return -1; /* invalid sort arguments */
    if (nmemb % n_0 == 0)
    {
        n_s = nmemb / n_0;
        n_l = n_0;
    }
    else
    {
        n_s = nmemb / n_0 + 1;
        n_l = nmemb % n_0;
    }
    /* warning1 ("(%d*%d+%d)", n_0, n_s-1, n_l); */

    /* now we cycle by gulps sorting porting of input file and
     writing them into temporary files */
    t0 = clock1 ();
    base = xmalloc (n_0 * size);
    /* warning1 ("("); */
    for (i=0; i<n_s; i++)
    {
        /* set the size of the portion */
        if (i == n_s-1)  n = n_l;
        else             n = n_0;
        /* warning1 ("%d:%d,", i, n); */
        /* read the portion and sort it */
        xread (fh_in, base, n*size);
        sort1 (base, n, size, compar);
        /* write sorted portion into the temporary file */
        fh = xcreate ("%s/%d-%d", tmp_path, pid, i);
        xwrite (fh, base, n*size);
        close (fh);
    }
    /* warning1 (")\n"); */
    /* warning1 ("memory sort: %.3f sec, ", clock1() - t0); */

    /* open temporary files and initialize merging data structures */
    f = xmalloc (sizeof(f[0]) * n_s);
    for (i=0; i<n_s; i++)
    {
        f[i].fh = xopen ("%s/%d-%d", tmp_path, pid, i);
        f[i].n = n_0;
        f[i].ci = 0;
        f[i].e = xmalloc (size);
        xread (f[i].fh, f[i].e, size);
    }
    f[n_s-1].n = n_l;

    /* merge temporary files into one. now we have n_s sources of items
     (last one will be exhausted sooner than others). 'base' is used
     as a write cache */
    t0 = clock1 ();
    nsrc = n_s;
    n1 = 0;
    nw = 0;
    while (nsrc)
    {
        /* warning1 ("%d: [", nsrc); */
        /* for (i=0; i<n_s; i++) */
        /* { */
        /*    if (f[i].n == f[i].ci) */
        /*        warning1 ("-,"); */
        /*    else */
        /*        warning1 ("%d:%d,", f[i].n, f[i].ci); */
        /* } */
        /* warning1 ("]"); */
        /* look up the smallest of all sources */
        j = -1;
        for (i=0; i<n_s; i++)
        {
            /* is this source valid? */
            if (f[i].ci != f[i].n)
            {
                /* nothing in sleeve yet? */
                if (j == -1)
                {
                    j = i;
                }
                /* see if i-th element is smaller than j-th */
                else if (compar(f[i].e, f[j].e) < 0)
                {
                    j = i;
                }
            }
        }
        /* warning1 ("j=%d", j); */
        /* see if we found something. if not, this is fatal error */
        if (j == -1) error1 ("fsort() fatal internal error\n");
        /* now we output the least element  */
        memcpy (((char *)base)+nw*size, f[j].e, size);
        nw++;
        if (nw == n_0)
        {
            xwrite (fh_out, base, nw*size);
            nw = 0;
        }
        /* xwrite (fh_out, f[j].e, size); */
        f[j].ci++;
        n1++;
        /* see if this source is exhausted */
        if (f[j].ci == f[j].n)
        {
            nsrc--;
        }
        else
        {
            xread (f[j].fh, f[j].e, size);
        }
        /* warning1 ("\n"); */
    }
    xwrite (fh_out, base, nw*size);
    if (n1 != nmemb)
        error1 ("fsort() fatal internal inconsistency; "
                "%d items written but should have %d\n", n1, nmemb);
    /* warning1 ("merge: %.3f sec\n", clock1() - t0); */
    free (base);

    /* close and remove unneeded temporary files */
    for (i=0; i<n_s; i++)
    {
        free (f[i].e);
        close (f[i].fh);
        xunlink ("%s/%d-%d", tmp_path, pid, i);
    }

    return 0;
}

int fsort (int fh_in, int fh_out, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *),
           void (*sort1)(void *base, size_t nmemb, size_t size, int (*compar1)(const void *, const void *)),
           int msize, char *tmp_path)
{
    void   *base, *base1;
    int    fh;
    unsigned int nw, i, j, nsrc, n_0, n_l, n, n_s, pid, n1, nbuf, n_1;
    double t0;
    struct f_t
    {
        int  fh; /* file handle */
        int  ci; /* current index while merging */
        int  n;  /* number of elements in the file */
        void *e;
    }
    *f;

    /* see if we can do memory-based sort (i.e. specified memory size is
     enough to hold entire data array) */
    if (nmemb*size <= msize)
    {
        /* warning1 ("entire array will be sorted in memory\n"); */
        base = xmalloc (nmemb*size);
        xread (fh_in, base, nmemb*size);
        sort1 (base, nmemb, size, compar);
        xwrite (fh_out, base, nmemb*size);
        free (base);
        return 0;
    }

    pid = getpid ();
    
    /* find out how many entries (n_0) we will be able to sort in one gulp
     i.e., in memory with sort1(), how many steps it will take (ns), and
     what will be the size of the last gulp (n_l) */
    n_0 = msize / size;
    if (n_0 < 2) return -1; /* invalid sort arguments */
    if (nmemb % n_0 == 0)
    {
        n_s = nmemb / n_0;
        n_l = n_0;
    }
    else
    {
        n_s = nmemb / n_0 + 1;
        n_l = nmemb % n_0;
    }
    /* warning1 ("(%d*%d+%d)", n_0, n_s-1, n_l); */

    /* now we cycle by gulps sorting portions of input file and
     writing them into temporary files */
    t0 = clock1 ();
    base = xmalloc (n_0 * size);
    /* warning1 ("("); */
    for (i=0; i<n_s; i++)
    {
        /* set the size of the portion */
        if (i == n_s-1)  n = n_l;
        else             n = n_0;
        /* warning1 ("%d:%d,", i, n); */
        /* read the portion and sort it */
        xread (fh_in, base, n*size);
        sort1 (base, n, size, compar);
        /* write sorted portion into the temporary file */
        fh = xcreate ("%s/%d-%d", tmp_path, pid, i);
        xwrite (fh, base, n*size);
        close (fh);
    }
    /* warning1 (")\n"); */
    /* warning1 ("memory sort: %.3f sec, ", clock1() - t0); */

    /* find out how long every merging input buffer will be. half of n_0 items
     is allocated to merging output buffer and the other half is for
     'n_s' merging input buffers */
    nbuf = n_0 / 2 / n_s;
    if (nbuf < 1) nbuf = 1;
    n_1 = n_0 - nbuf*n_s;
    base1 = (char *)base + nbuf*size*n_s;

    /* open temporary files and initialize merging data structures */
    f = xmalloc (sizeof(struct f_t) * n_s);
    for (i=0; i<n_s; i++)
    {
        f[i].fh = xopen ("%s/%d-%d", tmp_path, pid, i);
        /* f[i].n = nbuf; */
        f[i].ci = 0;
        /* f[i].e = xmalloc (size); */
        f[i].e = (char *)base + nbuf*size*i;
        f[i].n = read (f[i].fh, f[i].e, nbuf*size);
        if (f[i].n < 0)
            error1 ("failed to read %d bytes from %s/%d-%d\n", nbuf*size, tmp_path, pid, i);
        f[i].n /= size;
    }

    /* merge temporary files into one. now we have n_s sources of items
     (last one will be exhausted sooner than others). 'base1' is used
     as a write cache */
    t0 = clock1 ();
    nsrc = n_s;
    n1 = 0;
    nw = 0;
    while (nsrc)
    {
        /* warning1 ("%d: [", nsrc); */
        /* for (i=0; i<n_s; i++) */
        /* { */
        /*    if (f[i].n == f[i].ci) */
        /*        warning1 ("-,"); */
        /*    else */
        /*        warning1 ("%d:%d,", f[i].n, f[i].ci); */
        /* } */
        /* warning1 ("]"); */
        /* look up the smallest of all sources */
        j = -1;
        for (i=0; i<n_s; i++)
        {
            /* is this source valid? */
            if (f[i].ci != -1)
            {
                /* nothing in sleeve yet? */
                if (j == -1)
                {
                    j = i;
                }
                /* see if i-th element is smaller than j-th */
                else if (compar(f[i].e, f[j].e) < 0)
                {
                    j = i;
                }
            }
        }
        /* warning1 ("j=%d", j); */
        /* see if we found something. if not, this is fatal error */
        if (j == -1) error1 ("fsort() fatal internal error\n");
        /* now we output the least element  */
        memcpy (((char *)base1)+nw*size, f[j].e, size);
        nw++;
        if (nw == n_1)
        {
            xwrite (fh_out, base1, nw*size);
            nw = 0;
        }
        /* xwrite (fh_out, f[j].e, size); */
        f[j].ci++;
        n1++;

        /* see if this source is exhausted */
        if (f[j].ci == f[j].n)
        {
            /* nsrc--; */
            f[j].n = read (f[j].fh, f[j].e, nbuf*size);
            if (f[j].n < 0)
                error1 ("failed to read %d bytes from %s/%d-%d", nbuf*size, tmp_path, pid, j);
            if (f[j].n != 0)
            {
                f[j].n /= size;
                f[j].ci = 0;
            }
            else
            {
                f[j].ci = -1;
                nsrc--;
            }
        }
        /* warning1 ("\n"); */
    }
    xwrite (fh_out, base, nw*size);
    if (n1 != nmemb)
        error1 ("fsort() fatal internal inconsistency; "
                "%d items written but should have %d\n", n1, nmemb);
    /* warning1 ("merge: %.3f sec\n", clock1() - t0); */
    free (base);

    /* close and remove unneeded temporary files */
    for (i=0; i<n_s; i++)
    {
        close (f[i].fh);
        xunlink ("%s/%d-%d", tmp_path, pid, i);
    }

    return 0;
}

/* removes repetitive elements from binary file 
 * Parameters:
 * fh_in, fh_out -- must be preopened handles for input/output files;
 * nmemb, size -- number of elements in input stream and size (in bytes)
 *       of each element, just like in qsort();
 * compar -- function to compare elements, like in qsort();
 */
int funiq (int fh_in, int fh_out, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    void  *e1, *e2;
    int   i, n1;

    if (nmemb <= 0) return 0;

    e1 = xmalloc (size);
    e2 = xmalloc (size);

    xread (fh_in, e1, size);
    xwrite (fh_out, e1, size);
    n1 = 1;
    for (i=1; i<nmemb; i++)
    {
        xread (fh_in, e2, size);
        if (compar (e1, e2) == 0) continue;
        xwrite (fh_out, e2, size);
        memcpy (e1, e2, size);
        n1++;
    }

    free (e1);
    free (e2);

    return n1;
}

#define elem(i) ( ((char *)(bases[i])) + ci[i]*size )

/* merges n arrays into one array which is either intersection
 (op=1) or union (op=2) of the original arrays 'bases'. all
 operations are done in memory. result is malloc()ed, and
 its length (or -1 if error) is returned. nmembs[i] is a number
 of elements in bases[i], 'size' is a size (in bytes) of each
 element */
int merge (int op, int n, void **bases, int *nmembs, size_t size,
           void **result, int (*compar)(const void *, const void *))
{
    int          i, j, k, nsrc, n1, *ci, equal, rc;
    unsigned int nr;
    void         *r, *e;

    /* first we check the operation */
    if (op != 1 && op != 2) return -1;

    /* then we count how large resulting array could be */
    if (op == 1)
    {
        nr = nmembs[0];
        for (i=1; i<n; i++) nr = min1 (nr, nmembs[i]);
        if (nr == 0) return 0;
    }
    else
    {
        nr = 0;
        for (i=0; i<n; i++) nr += nmembs[i];
    }

    /* allocate space for resulting array */
    r = xmalloc (size * nr);
    e = NULL;

    /* allocate space for 'current indexes', one per array */
    ci = xmalloc (sizeof(int) * n);
    for (i=0; i<n; i++) ci[i] = 0;

    /* merge arrays */
    n1 = 0;
    if (op == 2)
    {
        nsrc = n;
        /* make sure we don't count empty arrays */
        for (i=0; i<n; i++) if (nmembs[i] == 0) nsrc--;
        while (nsrc)
        {
            /* look up the smallest of all sources */
            j = -1;
            for (i=0; i<n; i++)
            {
                /* is this source valid? */
                if (ci[i] != nmembs[i])
                {
                    /* nothing in sleeve yet? */
                    if (j == -1)
                    {
                        j = i;
                    }
                    /* see if i-th element is smaller than j-th */
                    else if (compar(elem(i), elem(j)) < 0)
                    {
                        j = i;
                    }
                }
            }
            /* see if we found something. if not, this is fatal error */
            if (j == -1) error1 ("merge() fatal internal error\n");
            /* now we output the least element  */
            if (e == NULL)
            {
                e = xmalloc (size);
            }
            else
            {
                if (compar (e, elem(j)) == 0) goto next;
            }
            memcpy (e, elem(j), size);
            memcpy (((char *)r)+n1*size, elem(j), size);
            n1++;
        next:
            ci[j]++;
            /* see if this source is exhausted */
            if (ci[j] == nmembs[j])
            {
                nsrc--;
            }
        }
        if (e != NULL) free (e);
    }
    else
    {
        while (TRUE)
        {
            /* check if some sources are exhausted */
            for (i=0; i<n; i++) if (ci[i] >= nmembs[i]) break;
            if (i != n) break;

            /* see if all sources are equal, and simultaneously
             look up the smallest of them */
            j = 0;
            equal = TRUE;
            for (i=1; i<n; i++)
            {
                rc = compar (elem(i), elem(j));
                if (rc)
                {
                    if (rc < 0) j = i;
                    equal = FALSE;
                }
            }
            if (equal)
            {
                /* when all inputs are equal, output the element */
                memcpy (((char *)r)+n1*size, elem(j), size);
                n1++;
                for (k=0; k<n; k++) ci[k]++;
                /* printf ("advancing all\n"); */
            }
            else
            {
                /* otherwise we only advance the smallest source */
                ci[j]++;
                /* printf ("advancing %d (%d)\n", j, ci[j]); */
            }
            /* for (k=0; k<n; k++) printf ("%d ", ci[k]); */
            /* printf ("\n"); */
            /* for (k=0; k<n; k++) printf ("%d ", *((int *)elem(k))); */
            /* printf ("\n"); */
        }
    }

    free (ci);
    *result = r;

    return n1;
}

#define elem1(index) (((char *)array)+size*(index))
#define elem2(index) (((char *)buffer)+size*(index))

/* deletes identical entries from the list; returns the final number of entries.
 arguments are the same as for qsort(). does not move discarded elements
 to the tail of the array so it is faster than uniq but cannot be
 used when elements contain malloc()ed memory, or leaks will occur. */
int uniq (void *array, size_t nmemb, size_t size, int compare(__const__ void *, __const__ void *))
{
    int src, trg;

    if (nmemb <= 0) return 0;

    for (src=1, trg=1; src<nmemb; src++)
    {
        if (compare (elem1(src), elem1(trg-1)))
        {
            if (src != trg)
                memcpy (elem1(trg), elem1(src), size);
            trg++;
        }
    }
    
    return trg;
}

/* deletes identical entries from the list; returns the final number of entries.
 arguments are the same as for qsort() */
int uniq2 (void *array, size_t nmemb, size_t size, int compare(__const__ void *, __const__ void *))
{
    int  src, trg, bufsize, n, n0;
    void *buffer;

    if (nmemb <= 0) return 0;

    /* allocate aside-buffer for temporarily removed elements */
    bufsize = max1 (1, min1 (nmemb, 16*1024*1024/size));
    buffer = xmalloc (bufsize * size);
    n0 = nmemb;

    n = 0; /* number of elements in the aside-buffer */
    for (src=1, trg=1; src<nmemb; src++)
    {
        /* see if aside-buffer is full */
        if (n == bufsize)
        {
            memmove (elem1(trg), elem1(src), size*(nmemb-src));
            memcpy (elem1(nmemb-n), buffer, size*n);
            n = 0;
            nmemb -= bufsize;
            src = trg;
        }

        if (compare (elem1(src), elem1(trg-1)))
        {
            if (src != trg)
                memcpy (elem1(trg), elem1(src), size);
            trg++;
        }
        else
        {
            memcpy (elem2(n), elem1(src), size);
            n++;
        }
    }
    if (n != 0)
        memcpy (elem1(nmemb-n), buffer, size*n);

    free (buffer);
    return trg;
}

/* partial sort. sorts 'array' using 'compare' but instead of
 sorting entire array it will only put 'limit' first smallest
 elements. the rest of an array is left in arbitrary order */
int psort (void *array, size_t nmemb, size_t size, int limit,
           int compare(__const__ void *, __const__ void *))
{
    int   i, j, n1, nsorted;
    void  *temp;

    /* sanity check: limit < nmemb */
    if (limit <= 0 || limit >= nmemb)
    {
        qsort (array, nmemb, size, compare);
        return nmemb;
    }

    temp = xmalloc (size);
    /* at the start of the procedure we have one element in our */
    /* subarray of sorted elements (nsorted=1) */
    nsorted = 1;

    for (i=1; i<nmemb; i++)
    {
        if (nsorted < limit ||
            compare (elem1(i), elem1(nsorted-1)) < 0)
        {
            memcpy (temp, elem1(i), size);
            for (j=0; j<nsorted; j++)
            {
                if (compare (temp, elem1(j)) < 0) break;
            }
            /* see how many element we will have to move */
            /* to give space for inserted element */
            /* example: nsorted = 4, j = 1, limit = 10 (nsorted<limit) */
            /* "1 3 4 5" inserting 2 */
            /* final subarray: "1 2 3 4 5" */
            /* 3 elements (nsorted-j) were moved */
            /* example: nsorted = 4, j = 1, limit = 4 (nsorted==limit) */
            /* "1 3 4 5" inserting 2 */
            /* final subarray: "1 2 3 4" */
            /* 2 elements (nsorted-j-1) were moved */
            n1 = nsorted < limit ? nsorted-j : nsorted-j-1;
            /* put i-th element (temp) into j-th cell */
            /* and move the rest of the array down the */
            /* line discarding element 'nsorted-1' when */
            /* sorted subarray is full */
            memmove (elem1(j+1), elem1(j), size*n1);
            memcpy (elem1(j), temp, size);
            if (nsorted < limit) nsorted++;
        }
    }
    free (temp);

    return nsorted;
}

/* partial sort/uniq. sorts 'array' using 'compare' but instead of
 sorting entire array it will only put 'limit' first smallest
 elements. the rest of an array is left in arbitrary order */
int pusort (void *array, size_t nmemb, size_t size, int limit,
           int compare(__const__ void *, __const__ void *))
{
    int   i, j, c = 0, n1, nsorted;
    void  *temp;

    /* sanity check: limit < nmemb */
    if (limit <= 0 || limit >= nmemb)
    {
        qsort (array, nmemb, size, compare);
        return nmemb;
    }

    temp = xmalloc (size);
    /* at the start of the procedure we have one element in our */
    /* subarray of sorted elements (nsorted=1) */
    nsorted = 1;

    for (i=1; i<nmemb; i++)
    {
        if (nsorted < limit ||
            compare (elem1(i), elem1(nsorted-1)) < 0)
        {
            memcpy (temp, elem1(i), size);
            for (j=0; j<nsorted; j++)
            {
                c = compare(temp, elem1(j));
                if (c <= 0) break;
            }
            if (c == 0) continue;
            /* see how many element we will have to move */
            /* to give space for inserted element */
            /* example: nsorted = 4, j = 1, limit = 10 (nsorted<limit) */
            /* "1 3 4 5" inserting 2 */
            /* final subarray: "1 2 3 4 5" */
            /* 3 elements (nsorted-j) were moved */
            /* example: nsorted = 4, j = 1, limit = 4 (nsorted==limit) */
            /* "1 3 4 5" inserting 2 */
            /* final subarray: "1 2 3 4" */
            /* 2 elements (nsorted-j-1) were moved */
            n1 = nsorted < limit ? nsorted-j : nsorted-j-1;
            /* put i-th element (temp) into j-th cell */
            /* and move the rest of the array down the */
            /* line discarding element 'nsorted-1' when */
            /* sorted subarray is full */
            memmove (elem1(j+1), elem1(j), size*n1);
            memcpy (elem1(j), temp, size);
            if (nsorted < limit) nsorted++;
        }
    }
    free (temp);

    return nsorted;
}

/* this function will try to bracket left and right elements in 'array'
 which bracket 'key', i.e. 'left' <= 'key' <= 'right' according 'compare'
 function. 'nmemb' and 'size' have the same meaning as in bsearch(). returns:
 0 if key is bracketed; 1 if key is found in the array L and R
 show all elements which are equal to key; -1 if key value is smaller than
 smallest (left) element, -2 if key value is larger than largest (right)
 element, -3 if array is empty */
int bracket (const void *key, void *array, size_t nmemb, size_t size,
             int compare(__const__ void *, __const__ void *),
             int *left, int *right)
{
    int L, R, M, L1, R1, M1, c, c1, c2;

    /* we can't search in empty array */
    if (nmemb == 0) return -3;

    /* at first we set left=0, right=nmemb-1 */
    L = 0;
    R = nmemb-1;

    /* check error condition: L must be >= key, R must be <= key */
    c1 = compare (elem1(L), key);
    c2 = compare (elem1(R), key);
    if (c1 > 0) return -1;
    if (c2 < 0) return -2;

    /* set 'left/right boundary matches' flags */
    if (c1 == 0 && c2 == 0)
    {
        *left = L;
        *right = R;
        return 1;
    }
    if (c1 == 0)
    {
        M = L;
        goto found_key;
    }
    if (c2 == 0)
    {
        M = R;
        goto found_key;
    }

    /* now try to compact the range */
    while (R-L > 1)
    {
        /* determine the middle of the bracket and compare it to key */
        M = (R + L) / 2;
        c = compare (elem1(M), key);
        /* if we hit the key we switch to other mode */
        if (c == 0) goto found_key;
        /* set left or right boundary according comparison result */
        if (c < 0) L = M;
        else       R = M;
    }

    *left  = L;
    *right = R;
    return 0;

found_key:
    /* now we bracket left and right boundary separately */
    L1 = L; R1 = M;
    while (R1-L1 > 1) /* L1<key, R1==key */
    {
        /* determine the middle of the bracket and compare it to key */
        M1 = (R1 + L1) / 2;
        c = compare (elem1(M1), key);
        /* if we hit the key this becomes R1. note that key cannot be > M1 */
        if (c == 0) R1 = M1;
        /* set left boundary otherwise */
        else        L1 = M1;
    }
    *left = R1;

    L1 = M; R1 = R;
    while (R1-L1 > 1) /* L1==key, R1>key */
    {
        /* determine the middle of the bracket and compare it to key */
        M1 = (R1 + L1) / 2;
        c = compare (elem1(M1), key);
        /* if we hit the key this becomes L1. note that key cannot be < M1 */
        if (c == 0) L1 = M1;
        /* set right boundary otherwise */
        else        R1 = M1;
    }
    *right = L1;

    return 1;
}

/* returns 0 on success, -1 on error (not enough memory, can't read),
 * 1 if input stream is empty. when return value is 0, memory buffers have
 * been allocated and have to be freed using linebuf_close() */
int linebuf_init (linebuf_t *lb, FILE *fp, int msize)
{
    lb->fp = fp;
    lb->msize = msize;
    lb->buf = malloc (msize);
    if (lb->buf == NULL) return -1;

    /* read first portion. don't look for line ends yet */
    lb->nb = fread (lb->buf, 1, lb->msize, fp);
    if (lb->nb < 0)
    {
        free (lb->buf);
        return -1;
    }
    if (lb->nb == 0)
    {
        free (lb->buf);
        return 1;
    }

    if (lb->nb < lb->msize)
    {
        fclose (lb->fp);
        lb->fp = NULL;
    }

    /* scanning pointer */
    lb->ns = 0;

    return 0;
}

/* returns 0 on success ('*s' points to next line extracted from stream,
 * don't free() it), -1 on error, 1 when input stream is empty. this
 * function does not close linebuf, even in case of error */
int linebuf_nextline (linebuf_t *lb, char **s)
{
    int n;

    /* locate next end of line */
again:
    n = lb->ns;
    while (n < lb->nb && lb->buf[n] != '\n') n++;
    if (n == lb->nb)
    {
        /* end of the buffer was reached */
        if (lb->nb < lb->msize)
        {
            /* if the file has been exhausted, we return left bytes
             * as last line */
            if (lb->ns == lb->nb) return 1;
            *s = lb->buf + lb->ns;
            lb->buf[n] = '\0';
            lb->ns = n;
            return 0;
        }
        else
        {
            int leftover, nb;

            /* file has more data in it. read further */
            if (lb->ns == 0)
            {
                char *p;

                /* line is longer than buffer... have to extend the buffer */
                lb->msize *= 2;
                p = realloc (lb->buf, lb->msize);
                if (p == NULL) return -1;
                lb->buf = p;
                nb = fread (lb->buf+lb->msize/2, 1, lb->msize/2, lb->fp);
                if (nb < 0) return -1;
                lb->nb += nb;
                goto again;
            }
            /* move leftover to the start of the buffer and read more data */
            leftover = lb->nb - lb->ns;
            memmove (lb->buf, lb->buf+lb->ns, leftover);
            nb = fread (lb->buf+leftover, 1, lb->msize-leftover, lb->fp);
            if (nb < 0) return -1;
            lb->nb = leftover + nb;
            lb->ns = 0;
            /* try scanning again */
            goto again;
        }
    }
    *s = lb->buf + lb->ns;
    lb->buf[n] = '\0';
    lb->ns = n + 1;

    return 0;
}

int linebuf_close (linebuf_t *lb)
{
    if (lb->fp != NULL) fclose (lb->fp);
    free (lb->buf);

    return 0;
}

/* merge stream */
typedef struct
{
    int       marked;
    linebuf_t lb;
    char    *line; /* current line available. NULL if source is empty */
}
mstr_t;

/* sorts text file fn1, writes result to fn2. compare() is a function to
 compare pointers to strings, like cmp_str */
int sortfile (char *fn1, char *fn2,
              int compare(__const__ void *, __const__ void *), int msize,
              char *temp_path)
{
    char  *membuf, *p, **lp, fname[MAXPATHLEN];
    FILE  *fp, *fpc;
    pid_t pid = getpid ();
    int   i, j, nc, nb, nl, ns, rc, smallest, ln;
    int64_t offset;

    /* allocate working buffer */
    if (msize <= 0)
    {
        warning1 ("invalid msize %d in sortfile()\n", msize);
        return -1;
    }
    membuf = malloc (msize);
    if (membuf == NULL)
    {
        warning1 ("cannot allocate %d bytes\n", msize);
        return -1;
    }

    /* open input file */
    fp = fopen (fn1, "r");
    if (fp == NULL)
    {
        warning1 ("failed to open %s (%s)\n", fn1, strerror (errno));
        free (membuf);
        return -1;
    }

    /* cycle by chunks. sorted pieces are written into temporary files
     * in the current directory with names 'sortfile.$pid.$nchunk' */
    offset = 0;
    for (nc=0; ; nc++)
    {
        /* fill the buffer with piece of input file. parse buffer
         * into lines; use buffer tail to keep line pointers */

        if (fseeko (fp, offset, SEEK_SET) != 0 ||
            (nb = fread (membuf, 1, msize, fp)) < 0)
        {
            warning1 ("failed to seek/read on %s to %s\n", fn1, pretty64(offset));
            fclose (fp);
            free (membuf);
            return -1;
        }
        if (nb == 0) break;

        /* now we have 'nb' bytes in the buffer. process them: parse into
         * lines, put line pointers into the buffer tail until line scanning
         * and line pointers met */

        /* nl is the number of lines detected. (sizeof(char *)*nl) bytes
         * are occupied by line pointers, next pointer will be added
         * at msize-(nl+1)*sizeof(char *). */
        nl = 0;

        /* ns is the number of bytes scanned while searching for end
         * of line characters (\n). \r are stripped at the end of the
         * line */
        ns = 0;

        while (TRUE)
        {
            p = membuf + ns;
            while (*p != '\n' &&
                   p-membuf < nb &&
                   p-membuf < msize-(nl+1)*sizeof(char *)) p++;
            /* p now points past the end of the line or we have fallen off
             * the available data */
            if (p-membuf >= msize-(nl+1)*sizeof(char *) || p-membuf == nb)
            {
                /* we have hit the line pointers or end of data.
                 * stop scanning, set offset for the next piece */
                offset += ns;
                break;
            }
            else
            {
                /* terminate line by NUL. remove possible \r before \n.
                 * advance pointer */
                if (p > membuf && (*p-1) == '\r') *(p-1) = '\0';
                else                              *p     = '\0';
                p++;

                /* add found line to set of line pointers */
                lp = (char **)(membuf + msize - (nl+1)*sizeof(char *));
                *lp = membuf + ns;
                nl++;

                /* advance scanning pointer */
                ns = p - membuf;
            }
        }

        /* ensure that we have at least one line in the input buffer.
         * if this is not true, most likely we have hit input line
         * which is longer than input buffer! refuse to process
         * such situations */
        if (nl == 0)
        {
            warning1 ("no lines came out! too long lines or "
                      "too small buffer (%u bytes)\n", msize);
            fclose (fp);
            free (membuf);
            return -1;
        }

        /* sort lines */
        lp = (char **)(membuf + msize - nl*sizeof(char *));
        qsort (lp, nl, sizeof (char *), compare);

        /* write chunk file */
        snprintf (fname, sizeof (fname), "%s/sortfile.%u.%u", temp_path, (int)pid, nc);
        fpc = fopen (fname, "w");
        if (fpc == NULL)
        {
            warning1 ("cannot open chunk file %s (%s)\n", fname, strerror (errno));
            fclose (fp);
            free (membuf);
            return -1;
        }
        for (i=0; i<nl; i++)
        {
            ln = strlen (lp[i]);
            if (fwrite (lp[i], 1, ln, fpc) != ln ||
                fwrite ("\n", 1, 1, fpc)   != 1)
            {
                warning1 ("cannot write to %s (%s)\n", fname, strerror (errno));
                fclose (fpc);
                fclose (fp);
                free (membuf);
                return -1;
            }
        }
        if (fclose (fpc) != 0)
        {
            warning1 ("cannot close %s (%s)\n", fname, strerror (errno));
            fclose (fp);
            free (membuf);
            return -1;
        }
    }
    fclose (fp);

    /* OK, we have 'nc' chunks. time to merge them into 'fn2'.
     * if only one chunk is present, we try to rename it into final
     * file. if rename fails (usually due to different filesystems)
     * we have to copy it */
    if (nc == 0)
    {
        warning1 ("warning: empty input for sortfile (%s -> %s)\n", fn1, fn2);
        free (membuf);
        /* create empty output file */
        fp = fopen (fn2, "w");
        if (fp == NULL)
        {
            warning1 ("failed to open %s (%s)\n", fn2, strerror (errno));
            return -1;
        }
        fclose (fp);
        return 0;
    }
    if (nc == 1)
    {
        snprintf (fname, sizeof (fname), "%s/sortfile.%u.%u", temp_path, (int)pid, 0);
        rc = rename (fname, fn2);
        if (rc != 0)
        {
            FILE *in, *out;
            int  nb;

            in = fopen (fname, "r");
            out = fopen (fn2, "w");
            if (in == NULL || out == NULL)
            {
                warning1 ("failed to open %s or %s (%s)\n",
                          fname, fn2, strerror (errno));
                if (in != NULL) fclose (in);
                if (out != NULL) fclose (out);
                free (membuf);
                return -1;
            }

            nb = file_length (fileno (in));
            if (copy_bytes (in, out, nb, msize, membuf) != 0)
            {
                warning1 ("failed to copy bytes from %s to %s\n", fname, fn2);
                fclose (in);
                fclose (out);
                free (membuf);
                return -1;
            }

            free (membuf);
            if (fclose (in) != 0)
            {
                warning1 ("failed to close %s (%s)\n", fname, strerror (errno));
                fclose (out);
                return -1;
            }
            if (fclose (out) != 0)
            {
                warning1 ("failed to close %s (%s)\n", fn2, strerror (errno));
                return -1;
            }
        }
        free (membuf);
    }
    else
    {
        int mcsize;
        mstr_t *M;

        free (membuf);

        fp = fopen (fn2, "w");
        if (fp == NULL)
        {
            warning1 ("failed to open %s (%s)\n", fn2, strerror (errno));
            return -1;
        }

        /* split available memory into 'nc' areas of equal size.
         * each area will be used as read cache for corresponding
         * chunk. we won't cache writing, consider libc caching
         * sufficient */
        M = xmalloc (sizeof (mstr_t) * nc);
        mcsize = max1 (msize / nc, 1024);
        for (i=0; i<nc; i++)
        {
            snprintf (fname, sizeof (fname), "%s/sortfile.%u.%u", temp_path, (int)pid, i);
            fpc = fopen (fname, "r");
            if (fpc == NULL)
            {
                for (j=0; j<i; j++)
                {
                    linebuf_close (&M[j].lb);
                    xunlink ("%s/sortfile.%u.%u", temp_path, pid, j);
                }
                free (M);
                fclose (fp);
                warning1 ("failed to open %s (%s)\n", fname, strerror (errno));
                return -1;
            }

            rc = linebuf_init (&M[i].lb, fpc, mcsize);
            if (rc != 0)
            {
                for (j=0; j<i; j++)
                {
                    linebuf_close (&M[j].lb);
                    xunlink ("%s/sortfile.%u.%u", temp_path, (int)pid, j);
                }
                free (M);
                fclose (fp);
                warning1 ("failed to open linebuf for %s; rc=%d\n", fname, rc);
                return -1;
            }
            M[i].marked = FALSE;

            /* set up first line */
            rc = linebuf_nextline (&M[i].lb, &M[i].line);
            if (rc < 0)
            {
                for (j=0; j<i; j++)
                {
                    linebuf_close (&M[j].lb);
                    xunlink ("%s/sortfile.%u.%u", temp_path, (int)pid, j);
                }
                free (M);
                fclose (fp);
                warning1 ("error fetching first line from %s\n", fname);
                return -1;
            }

            if (rc == 1)
                M[i].line = NULL;
        }

        /* perform merge */
        while (TRUE)
        {
            /* select smallest string in the merge set */
            smallest = -1;
            for (i=0; i<nc; i++)
            {
                if (M[i].line == NULL) continue;
                if (smallest < 0 || compare (&M[i].line, &M[smallest].line) < 0)
                {
                    smallest = i;
                }
            }
            if (smallest < 0) break;

            /* output smallest strings */
            for (i=0; i<nc; i++)
            {
                if (M[i].line == NULL) continue;
                if (compare (&M[i].line, &M[smallest].line) == 0)
                {
                    ln = strlen (M[i].line);
                    if (fwrite (M[i].line, 1, ln, fp) != ln ||
                        fwrite ("\n", 1, 1, fp) != 1)
                    {
                        for (j=0; j<nc; j++)
                        {
                            linebuf_close (&M[j].lb);
                            xunlink ("%s/sortfile.%u.%u", temp_path, (int)pid, j);
                        }
                        free (M);
                        fclose (fp);
                        warning1 ("error writing %s (%s)\n", fn2, strerror (errno));
                        return -1;
                    }
                    M[i].marked = TRUE;
                }
            }

            /* advance streams which were just output */
            for (i=0; i<nc; i++)
            {
                if (M[i].line == NULL) continue;
                if (M[i].marked)
                {
                    rc = linebuf_nextline (&M[i].lb, &M[i].line);
                    if (rc < 0)
                    {
                        for (j=0; j<nc; j++)
                        {
                            linebuf_close (&M[j].lb);
                            xunlink ("%s/sortfile.%u.%u", temp_path, (int)pid, j);
                        }
                        free (M);
                        fclose (fp);
                        warning1 ("error fetching next line from chunk %d\n", i);
                        return -1;
                    }
                    if (rc == 1) M[i].line = NULL;
                    M[i].marked = FALSE;
                }
            }
        }

        for (i=0; i<nc; i++) linebuf_close (&M[i].lb);
        free (M);

        /* remove chunks */
        for (i=0; i<nc; i++)
            xunlink ("%s/sortfile.%u.%u", temp_path, pid, i);

        if (fclose (fp) != 0)
        {
            warning1 ("failed to close %s (%s)\n", fn2, strerror (errno));
            return -1;
        }
    }

    return 0;
}

/* --- A piece from zettair ------------------------------------------------ */

/* macro to swap two data elements in place, one byte at a time.  I tried
 * replacing this with a duff device for optimisation, but it had no noticeable
 * effect. */
#define SWAP(one, two, size)                                                  \
    do {                                                                      \
        u_int8_t SWAP_tmp,                                                     \
               *SWAP_tone = (u_int8_t *) one,                                  \
               *SWAP_ttwo = (u_int8_t *) two;                                  \
        size_t SWAP_size = size;                                              \
        do {                                                                  \
            SWAP_tmp = *SWAP_tone;                                            \
            *SWAP_tone = *SWAP_ttwo;                                          \
            *SWAP_ttwo = SWAP_tmp;                                            \
            ++SWAP_tone;                                                      \
            ++SWAP_ttwo;                                                      \
        } while (--SWAP_size);                                                \
    } while (0)

void *heap_siftup (char *root, unsigned int element,
                   unsigned int size,
                   int (*cmp)(const void *one, const void *two))
{
    char *curr = root, *parent;

    while (element)
    {
        /* compare with parent */
        curr = root + size * element;
        element = (element - 1) / 2;
        parent = root + size * element;
        if (cmp(curr, parent) < 0)
        {
            /* swap element and parent and repeat */
            SWAP(curr, parent, size);
        } else
        {
            break;
        }
    }

    return curr;
}

/* sift an element that is out of heap order down the heap (from root to leaf
 * if necessary) */
void *heap_siftdown (char *element, char *endel,
                     unsigned int size, unsigned int diff,
                     int (*cmp)(const void *one, const void *two))
{
    char *lchild = element + diff;          /* note that the right child is one
                                             * array element greater than 
                                             * lchild */
    unsigned int right;                     /* whether right child is smaller */

    /* perform all heapifications where both children exist */
    while (lchild < endel) {
        /* compare left and right children */
        right = (cmp(lchild, lchild + size) > 0) * size;

        /* if current element is less extreme than more extreme child ... */
        if (cmp(element, lchild + right) > 0) {
            /* swap current element with greatest child */
            SWAP(element, lchild + right, size);
            element = lchild + right;

            /* calculate new child positions */
            diff = diff*2 + right;
            lchild = element + diff;
        } else {
            /* element is at its final position */
            break;
        }
    }

    /* perform (possible) heapification where only left child exists */
    if (lchild == endel) {
        /* if left child is more extreme than current element ... */
        if (cmp(element, endel) > 0) {
            /* swap current element with last element */
            SWAP(element, endel, size);
            return endel;
        }
    }    

    return element;
}

/* --- end of a piece from zettair ------------------------------------------ */


static int (*cmp_accumulate_f) (__const__ void *, __const__ void *);

static int cmp_accumulate (__const__ void *e1, __const__ void *e2)
{
    return - (*cmp_accumulate_f) (e1, e2);
}

/* adds 'elem' to 'array' if 'elem' is smaller than largest element in
 * the array. returns 0 if element is skipped, 1 if added, negative
 * value on error */
int accumulate (void *array, void *elem, size_t size, 
                size_t *nmemb, size_t max_nmemb,
                int compare(__const__ void *, __const__ void *))
{
    cmp_accumulate_f = compare;

    /* accumulator is full? */
    if (*nmemb == max_nmemb)
    {
        /* if new element is smaller than smallest one, simply skip it */
        if (cmp_accumulate (elem, array+0) < 0) return 0;

        /* if heap is full, we replace smallest element */
        memcpy (array+0, elem, size);

        heap_siftdown (array, array+(*nmemb-1)*size, size, size, cmp_accumulate);
    }
    else
    {
        /* if heap is not full, we append new element to the heap */
        memcpy (array+(*nmemb)*size, elem, size);
        (*nmemb)++;

        heap_siftup (array, (*nmemb)-1, size, cmp_accumulate);
    }

    return 1;
}

/* partial sort. sorts 'array' using 'compare' but instead of
 * sorting entire array it will only put not more than 'limit'
 * first smallest elements. 
 * returns number of elements in sorted subarray */
int psort1 (void *array, size_t nmemb, size_t size, int limit,
            int compare(__const__ void *, __const__ void *))
{
    int i;
    size_t n = 0;
    void *arrtmp = xmalloc (size * limit);

    if (nmemb < 100 || limit >= nmemb/3)
    {
        qsort (array, nmemb, size, compare);
        return min1 (nmemb, limit);
    }

    for (i=0; i<nmemb; i++)
    {
        accumulate (arrtmp, array+size*i, size, &n, limit, compare);
    }
    qsort (arrtmp, n, size, compare);
    memcpy (array, arrtmp, size * n);

    return n;
}
