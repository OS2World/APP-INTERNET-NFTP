#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

intcacheparms_t intcache = {0, 1024*1024, 0, 0, 0.0};

/* wrapper for getting 64-bit number */
int get64_integer (int fh, int64_t *num)
{
    int rc;
    unsigned int n1, n2;

    rc = get_integer (fh, &n1);
    if (rc != 0) error1 ("get64_integer() failed: file exhausted\n");
    rc = get_integer (fh, &n2);
    if (rc != 0) error1 ("get64_integer() failed: file exhausted\n");

    *num = 4294967296LL * (int64_t) n2 + (int64_t) n1;

    return 0;
}

/* retrieves next integer from the input stream  */
int get_integer (int fh, unsigned int *num)
{
    static int *buffer = NULL;
    static int bufsize, nr, ng;
    double t1;

    unsigned int nb;

    /* special case: free buffer if fh == -1 */
    if (fh == -1)
    {
        if (intcache.membabble)
            warning1 ("GETINT: freeing buffer\n");
        if (buffer != NULL) free (buffer);
        buffer = NULL;
        return 0;
    }

    /* initializations on first invocation */
    if (buffer == NULL)
    {
        if (intcache.buffersize == 0) error1 ("internal error: intcache.buffersize is not set!\n");
        /* size of the buffers must be multiple of sizeof(int) */
        bufsize = (intcache.buffersize/2/sizeof(int)) * sizeof(int);
        if (intcache.membabble)
            warning1 ("GETINT: allocating %s bytes for read cache\n", pretty(bufsize*sizeof(int)));
        buffer = xmalloc (bufsize);
        nr = 0; /* number of integers already in the buffer */
        ng = 0; /* number of integers already returned */
    }

    /* if buffer is empty, fill it */
    if (ng == nr)
    {
        t1 = clock1();
        nb = read (fh, buffer, bufsize);
        intcache.iotime += clock1() - t1;
        if (nb < 0) error1 ("get_integer: error reading from input file\n");
        if (nb == 0) return 1; /* normal end */
        nr = nb / sizeof(int);
        ng = 0;
    }

    *num = buffer[ng++];

    return 0;
}

/* retrieves next integer from the n-th of the N input streams  */
int nget_integer (int N, int n, int fh, unsigned int *num)
{
    static int **buffers = NULL;
    static int i, bufsize, *nr, *ng;
    double t1;

    unsigned int nb;

    /* special case: free buffer if fh == -1 */
    if (fh == -1)
    {
        if (intcache.membabble)
            warning1 ("NGETINT: freeing buffer(%d)\n", n);
        if (buffers[n] != NULL) free (buffers[n]);
        buffers[n] = NULL;
        return 0;
    }

    /* special case: free everything when n == -1 */
    if (n == -1)
    {
        if (intcache.membabble)
            warning1 ("NGETINT: freeing buffers\n");
        free (buffers);
        free (nr);
        free (ng);
        buffers = NULL;
        return 0;
    }

    /* initializations on first invocation */
    if (buffers == NULL)
    {
        buffers = xmalloc (sizeof(char *) * N);
        nr = xmalloc (sizeof(int) * N);
        ng = xmalloc (sizeof(int) * N);
        /* we allocate 1/2 of memory to write cache and 1/2 of memory to
         read cache. read cache is divided to N buffers, one buffer
         per input stream */
        if (intcache.buffersize == 0) error1 ("internal error: intcache.buffersize is not set!\n");
        bufsize = (intcache.buffersize/2/N/sizeof(int)) * sizeof(int);
        if (intcache.membabble)
            warning1 ("NGETINT: allocating %u buffers, %s bytes each for read cache\n",
                      N, pretty(bufsize*sizeof(int)));
        for (i=0; i<N; i++)
        {
            buffers[i] = xmalloc (bufsize);
            nr[i] = 0; /* number of integers already in the buffer */
            ng[i] = 0; /* number of integers already returned */
        }
    }

    /* if buffer is empty, fill it */
    if (ng[n] == nr[n])
    {
        t1 = clock1();
        nb = read (fh, buffers[n], bufsize);
        intcache.iotime += clock1() - t1;
        if (nb < 0) error1 ("get_integer: error reading from input file\n");
        if (nb == 0) return 1; /* normal end */
        nr[n] = nb / sizeof(int);
        ng[n] = 0;
    }

    *num = buffers[n][ng[n]++];

    return 0;
}

/* wrapper for putting 64-bit number */
int put64_integer (int fh, int64_t num)
{
    unsigned int n1, n2;

    n1 = num % 4294967296LL;
    n2 = num / 4294967296LL;
    put_integer (IC_PUT, fh, n1, NULL);
    put_integer (IC_PUT, fh, n2, NULL);

    return 0;
}

/* operates write cache */
int put_integer (int mode, int fh, unsigned int docid, int64_t *loc)
{
    static int      *buffer = NULL;
    static int      bufsize, nw = 0;
    static int64_t  pos = 0;
    double          t1;

    /* initializations on first invocation */
    if (buffer == NULL)
    {
        if (intcache.buffersize == 0) error1 ("internal error: intcache.buffersize is not set!\n");
        /* size of the buffers must be multiple of sizeof(int) */
        bufsize = intcache.buffersize/2 / sizeof(int);
        if (intcache.membabble)
            warning1 ("PUTINT: allocating %s bytes for write cache\n", pretty(bufsize*sizeof(int)));
        buffer = xmalloc (bufsize * sizeof(int));
        nw = 0; /* number of integers stored in the buffer */
        pos = 0; /* write position in the file (in bytes, ready for lseek) */
        intcache.sw = 0;
        intcache.bs = 0;
    }

    switch (mode)
    {
    case IC_PUT:
        /* if the buffer is full we write it, reset counter and advance write
         position */
        if (nw == bufsize)
        {
            t1 = clock1();
            xwrite (fh, buffer, nw*sizeof(int));
            intcache.iotime += clock1() - t1;
            pos += (int64_t) nw*sizeof(int);
            nw = 0;
        }
        /* store location if needed */
        if (loc != NULL) *loc = pos + (int64_t)nw*sizeof(int);
        /* put integer into buffer */
        buffer[nw++] = docid;
        break;

    case IC_SEEK:
        /* we must distinguish between two cases: */
        if (*loc >= pos)
        {
            /* 1. target position is inside the buffer */
            if (*loc-pos >= bufsize*sizeof(int))
                error1 ("internal error: seeking past the end of buffer\n");
            nw = (*loc - pos)/sizeof(int);
        }
        else
        {
            /* 2. target position is already written. drop buffer contents,
             move file pointer */
            nw = 0;
            t1 = clock1();
            xlseek (fh, *loc);
            intcache.iotime += clock1() - t1;
            intcache.bs++;
        }
        break;

    case IC_SET:
        /* we must distinguish between two cases: */
        if (*loc >= pos)
        {
            /* 1. target position is inside the buffer */
            if (*loc-pos >= bufsize*sizeof(int))
                error1 ("internal error: writing past the end of buffer.\n"
                        "pos=%s, loc=%s, ", printf64(pos), printf64(*loc));
            buffer[(*loc-pos)/sizeof(int)] = docid;
        }
        else
        {
            /* 2. target position is already written */
            t1 = clock1();
            xlseek (fh, *loc);
            xwrite (fh, &docid, sizeof(int));
            xlseek (fh, pos);
            intcache.iotime += clock1() - t1;
            intcache.sw++;
        }
        break;

    case IC_FLUSH:
        if (nw != 0)
        {
            t1 = clock1();
            xwrite (fh, buffer, nw*sizeof(int));
            intcache.iotime += clock1() - t1;
        }
        free (buffer);
        if (intcache.membabble)
            warning1 ("PUTINT: freeing buffer\n");
        buffer = NULL;
        break;
    }

    return 0;
}

/* operates write cache */
int nput_integer (int mode, int N, int n, int fh, unsigned int docid, int64_t *loc)
{
    static int      **buffers = NULL;
    static int      bufsize, *nw;
    static int64_t  *pos;
    int             i;
    double          t1;

    /* initializations on first invocation */
    if (buffers == NULL)
    {
        buffers = xmalloc (sizeof(char *) * N);
        nw = xmalloc (sizeof(int) * N);
        pos = xmalloc (sizeof(int64_t) * N);
        /* we allocate 1/2 of memory to write cache and 1/2 of memory to
         read cache. read cache is divided to N buffers, one buffer
         per input stream */
        if (intcache.buffersize == 0) error1 ("internal error: intcache.buffersize is not set!\n");
        bufsize = intcache.buffersize/2 / N / sizeof(int);
        if (intcache.membabble)
            warning1 ("NPUTINT: allocating %u buffers, %s bytes each for write cache\n",
                      N, pretty(bufsize*sizeof(int)));
        /* warning1 ("input buffer size is %d bytes per stream\n", bufsize*sizeof(int)); */
        for (i=0; i<N; i++)
        {
            buffers[i] = xmalloc (bufsize*sizeof(int));
            nw[i] = 0; /* number of integers stored in the buffer */
            pos[i] = 0;
        }
    }

    switch (mode)
    {
    case IC_PUT:
        /* if the buffer is full we write it, reset counter and advance write
         position */
        if (nw[n] == bufsize)
        {
            t1 = clock1();
            xwrite (fh, buffers[n], nw[n]*sizeof(int));
            intcache.iotime += clock1() - t1;
            pos[n] += (int64_t) nw[n]*sizeof(int);
            nw[n] = 0;
        }
        /* store location if needed */
        if (loc != NULL) *loc = pos[n] + (int64_t)nw[n]*sizeof(int);
        /* put integer into buffer */
        buffers[n][nw[n]] = docid;
        nw[n]++;
        break;

    case IC_FLUSH:
        if (nw[n] != 0)
        {
            t1 = clock1();
            xwrite (fh, buffers[n], nw[n]*sizeof(int));
            intcache.iotime += clock1() - t1;
        }
        free (buffers[n]);
        if (intcache.membabble)
            warning1 ("NPUTINT: freeing buffer(%d)\n", n);
        buffers[n] = NULL;
        break;

    case IC_SET:
        /* we must distinguish between two cases: */
        if (*loc >= pos[n])
        {
            /* 1. target position is inside the buffer */
            if (*loc-pos[n] >= bufsize*sizeof(int))
                error1 ("internal error: writing past the end of buffer.\n"
                        "pos=%s, loc=%s, ", printf64(pos[n]), printf64(*loc));
            buffers[n][(*loc-pos[n])/sizeof(int)] = docid;
        }
        else
        {
            /* 2. target position is already written */
            t1 = clock1();
            xlseek (fh, *loc);
            xwrite (fh, &docid, sizeof(int));
            xlseek (fh, pos[n]);
            intcache.iotime += clock1() - t1;
        }
        break;

    case IC_FREE:
        if (intcache.membabble)
            warning1 ("NPUTINT: freeing buffers\n");
        free (buffers);
        free (nw);
        free (pos);
        break;
    }

    return 0;
}
