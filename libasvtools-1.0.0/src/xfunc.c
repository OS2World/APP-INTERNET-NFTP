#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*#define NO_DMALLOC_HERE*/

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifndef __MINGW32__
#include <sys/param.h>
#else
#define MAXPATHLEN 260
#endif

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int xcreate (char *format, ...)
{
    int       fh;
    va_list   args;
    char      buffer[MAXPATHLEN];

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);
    
    fh = open (buffer, O_WRONLY|O_TRUNC|O_CREAT|BINMODE, 0666);
    if (fh < 0) error1 ("cannot create/truncate %s\n", buffer);
    return fh;
}

int xopen (char *format, ...)
{
    int       fh;
    va_list   args;
    char      buffer[MAXPATHLEN];

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);

    fh = open (buffer, BINMODE|O_RDONLY);
    if (fh < 0) error1 ("cannot open %s\n", buffer);

    return fh;
}

FILE *xfopen (char *format, char *mode, ...)
{
    FILE      *fp;
    va_list   args;
    char      buffer[MAXPATHLEN];

    va_start (args, mode);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);

    fp = fopen (buffer, mode);
    if (fp == NULL) error1 ("cannot open %s in mode \"%s\" (%s)\n",
                            buffer, mode, strerror (errno));

    return fp;
}

int xfclose (FILE *fp)
{
    if (fclose (fp) != 0)
        error1 ("cannot close file stream (%s)\n", strerror (errno));

    return 0;
}

int xfwrite (void *buffer, int nb, FILE *fp)
{
    int nbw;

    nbw = fwrite (buffer, 1, nb, fp);
    if (nbw != nb)
        error1 ("error writing: wanted to write %d bytes, wrote %d (%s)\n",
                nb, nbw, strerror (errno));

    return nbw;
}

int xfread (void *buffer, int nb, FILE *fp)
{
    int nbr;

    nbr = fread (buffer, 1, nb, fp);
    if (nbr < 0)
        error1 ("error reading (%s)\n", strerror (errno));

    return nbr;
}

int xwrite (int fd, void *buffer, int size)
{
    int n;

    if (size == 0) return 0;
    n = write (fd, buffer, size);
    if (n != size) error1 ("error writing %d bytes\n", size);
    return 0;
}

int xwrite_str (int fd, char *s)
{
    int l;

    if (s == NULL) l = 0;
    else           l = strlen (s);

    xwrite (fd, &l, sizeof (int));
    if (l > 0)
        xwrite (fd, s, l+1);

    return 0;
}

int xwrite_str_array (int fd, char **s, int n)
{
    int i;

    xwrite (fd, &n, sizeof (int));
    for (i=0; i<n; i++)
        xwrite_str (fd, s[i]);

    return 0;
}

int xread (int fd, void *buffer, int size)
{
    int n;

    if (size == 0) return 0;
    n = read (fd, buffer, size);
    if (n != size) error1 ("error reading %d bytes\n", size);
    return 0;
}

char *xread_str (int fd)
{
    char *s;
    int  l;

    xread (fd, &l, sizeof (int));
    s = xmalloc (l+1);
    if (l > 0)
        xread (fd, s, l+1);

    return s;
}

int xread_str_array (int fd, char ***s)
{
    int i, n;

    xread (fd, &n, sizeof (int));
    *s = malloc (sizeof (char **) * n);

    for (i=0; i<n; i++)
        (*s)[i] = xread_str (fd);

    return n;
}

#ifndef DMALLOC
void *xmalloc (size_t n)
{
    void *ptr;

    /* if (n > 1000000) warning1 ("(%dMB)", n/1024/1024); */
    ptr = malloc (n);
    if (ptr == NULL) error1 ("not enough memory; malloc(%u) failed\n", n);
    return ptr;
}

void xfree (void *ptr)
{
    free (ptr);
}

void *xrealloc (void *ptr, size_t n)
{
    /* if (n > 1000000) warning1 ("[%dMB]", n/1024/1024); */
    if (n == 0)
    {
        free (ptr);
        ptr = NULL;
    }
    else if (ptr == NULL)
    {
        ptr = xmalloc (n);
    }
    else
    {
        ptr = realloc (ptr, n);
        if (ptr == NULL) error1 ("not enough memory; realloc(%u) failed\n", n);
    }
    return ptr;
}

char *xstrdup(char *str) {
	char *x=strdup(str);
	if (!x) error1 ("not enough memory; strdup %s failed\n", str);
	return x;
}
#endif

char *xnstrdup(char *str, int len) {
	char *x=(char*)xmalloc(len+1);
	memcpy(x, str, len);
	x[len]='\0';
	return x;
}

int xunlink (char *format, ...)
{
    va_list   args;
    char      buffer[MAXPATHLEN];

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);

    if (unlink (buffer) < 0) error1 ("cannot delete %s\n", buffer);
    return 0;
}

int yunlink (char *format, ...)
{
    va_list   args;
    char      buffer[MAXPATHLEN];

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);

    return unlink (buffer);
}

int xlseek (int fh, off_t offset)
{
    if (lseek (fh, offset, SEEK_SET) != offset)
        error1 ("lseek (%s) failed\n", pretty64(offset));

    return 0;
}

#if HAVE_MMAP

#include <sys/mman.h>

#ifndef MAP_FAILED
#define MAP_FAILED (void *)-1
#endif

void *xmmap (unsigned long *len, char *format, ...)
{
    int       fd;
    void      *p;
    va_list   args;
    char      buffer[MAXPATHLEN];

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);

    fd = open (buffer, O_RDONLY);
    if (fd < 0) error1 ("cannot open %s\n", buffer);
    *len = file_length (fd);
    p = mmap (NULL, *len, PROT_READ, MAP_SHARED, fd, 0);
    if (p == (void *)MAP_FAILED) error1 ("cannot mmap(%s)\n", buffer);
    close (fd);

    return p;
}

void xmunmap (void *addr, unsigned long len)
{
    munmap (addr, len);
}

#else

void *xmmap (unsigned long *len, char *format, ...)
{
    error1 ("mmap is not supported\n");
    return NULL;
}

void xmunmap (void *addr, unsigned long len)
{
    
}

#endif

/* this structure describes one set of memory chunks */
typedef struct
{
    int  nchunks, nchunks_a; /* number of chunks used, number of
    chunks allocated */
    char **chunks; /* array of memory chunks. its size is nchunks_a
    but only first 'nchunks' chunks are actually allocated. */
    int  init_size, bytes_used, bytes_left; /* initial chunk size,
    and number of bytes used and left in the last chunk */
}
chunk_set_t;

/* create a new set of memory chunks. returns an
 opaque pointer which must be later passed to chunk_* functions.
 'fragm_size' is a suggested size of the each chunk; use 0 for
 default value (1024KB). */
void *chunk_new (int fragm_size)
{
    chunk_set_t *pch;

    pch = xmalloc (sizeof (chunk_set_t));

    /* allocate chunk array */
    pch->nchunks_a = 16;
    pch->chunks = xmalloc (sizeof (char *) * pch->nchunks_a);
    pch->nchunks = 0;

    if (fragm_size <= 0) fragm_size = 1024*1024;
    pch->init_size = fragm_size;
    pch->bytes_used = 0;
    pch->bytes_left = 0;

    return pch;
}

/* put 'len' bytes of 'data' into chunk set 'pch'. if 'len' == -1,
 use strlen() on 'data' to determine data size and add one byte for
 terminating NUL */
void *chunk_put (void *p, char *data, int len)
{
    char        *p1;

    /* see how many bytes we need to store data */
    if (len == 0) error1 ("attempt to store 0 bytes in the memory chunk set\n");
    if (len < 0) len = strlen (data) + 1;

    p1 = chunk_alloc (p, len);

    /* warning1 ("writing bytes at %x\n", p1); */
    memcpy (p1, data, len);
    return p1;
}

/* allocate 'len' bytes in the chunk set 'pch' */
void *chunk_alloc (void *p, int len)
{
    chunk_set_t *pch = p;
    int         bufsize;
    char        *p1;

    /* see how many bytes we need to store data */
    if (len <= 0)
        error1 ("attempt to allocate %d bytes in the memory chunk set\n", len);

    /* warning1 ("entered chunk_put(%d); have %d chunks, %d bytes used, " */
    /*          "%d bytes left, chunk starts at %x, ", len, pch->nchunks, */
    /*          pch->bytes_used, pch->bytes_left, pch->chunks[pch->nchunks-1]); */
    /* check that we have enough bytes in the last chunk */
    if (len > pch->bytes_left)
    {
        if (pch->nchunks == pch->nchunks_a-1)
        {
            pch->nchunks_a *= 2;
            pch->chunks = xrealloc (pch->chunks, sizeof (char *) * pch->nchunks_a);
        }

        bufsize = max1 (pch->init_size, len);
        pch->chunks[pch->nchunks] = xmalloc (bufsize);
        /* warning1 ("\n->allocating %d bytes at %x\n", bufsize, pch->chunks[pch->nchunks]); */
        pch->bytes_used = 0;
        pch->bytes_left = bufsize;
        pch->nchunks++;
    }

    p1 = pch->chunks[pch->nchunks-1] + pch->bytes_used;
    pch->bytes_used += len;
    pch->bytes_left -= len;

    return p1;
}

/* frees all memory associated with given chunk set 'pch'. all pointers
 returned by chunk_put() become invalid. */
void chunk_free (void *p)
{
    chunk_set_t *pch = p;
    int         i;

    /* warning1 ("\nfreeing chunks!\n"); */
    for (i=0; i<pch->nchunks; i++)
        free (pch->chunks[i]);
    free (pch->chunks);

    free (pch);
}

/* ------------------------------------------------------------------------- */

#define MB_N 4

typedef struct
{
    int opened; /* this membuf is opened and allocated */
    int bufsize; /* memory chunk size */

    int nbufs, nbufs_a; /* how many buf pointers are used, how many
    are allocated */
    char **bufs;

    int left; /* bytes left in the last buffer */
}
membuf_t;

static int MB_init = FALSE;
static membuf_t MB[MB_N];

/* creates a set of output buffers in memory; nb is a size of one chunk.
 if n is zero or negative, 1MB is used. returns membuf id on success,
 negative value on error */
int membuf_create (int nb)
{
    int i;

    if (!MB_init)
    {
        memset (MB, 0, sizeof (membuf_t)*MB_N);
        MB_init = TRUE;
    }

    for (i=0; i<MB_N; i++)
    {
        if (!MB[i].opened)
        {
            MB[i].opened = TRUE;
            MB[i].bufsize = nb > 0 ? nb : 1024*1024;
            MB[i].nbufs = 1;
            MB[i].nbufs_a = 8;
            MB[i].bufs = xmalloc (sizeof (char *) * MB[i].nbufs_a);
            MB[i].bufs[0] = xmalloc (MB[i].bufsize);
            MB[i].left = MB[i].bufsize;
            return i;
        }
    }

    return -1;
}

/* destroy corresponding membuf */
int membuf_close (int mbid)
{
    int i;

    if (!MB_init || mbid < 0 || mbid >= MB_N || !MB[mbid].opened) return -1;

    for (i=0; i<MB[mbid].nbufs; i++) free (MB[mbid].bufs[i]);
    free (MB[mbid].bufs);

    MB[mbid].opened = FALSE;

    return 0;
}

/* put a NUL-terminated string into membuf (NUL is not copied) */
int membuf_put (int mbid, char *s)
{
    int ln;

    if (!MB_init || mbid < 0 || mbid >= MB_N || !MB[mbid].opened) return -1;

    ln = strlen (s);
    if (MB[mbid].left >= ln)
    {
        memcpy (MB[mbid].bufs[MB[mbid].nbufs-1] +
                (MB[mbid].bufsize - MB[mbid].left), s, ln);
        MB[mbid].left -= ln;
    }
    else
    {
        int copied;

        /* if the last buffer still has some bytes left, use them */
        if (MB[mbid].left > 0)
        {
            copied = MB[mbid].left;
            memcpy (MB[mbid].bufs[MB[mbid].nbufs-1] +
                    (MB[mbid].bufsize - MB[mbid].left), s, copied);
            ln -= copied;
            s += copied;
        }

        do
        {
            /* allocate next buffer */
            if (MB[mbid].nbufs_a == MB[mbid].nbufs)
            {
                MB[mbid].nbufs_a *= 2;
                MB[mbid].bufs = xrealloc (MB[mbid].bufs, sizeof (char *) * MB[mbid].nbufs_a);
            }

            MB[mbid].bufs[MB[mbid].nbufs] = xmalloc (MB[mbid].bufsize);
            MB[mbid].nbufs++;
            MB[mbid].left = MB[mbid].bufsize;

            copied = min1 (MB[mbid].bufsize, ln);
            memcpy (MB[mbid].bufs[MB[mbid].nbufs-1] +
                    (MB[mbid].bufsize - MB[mbid].left), s, copied);
            MB[mbid].left -= ln;
            ln -= copied;
            s += copied;
        }
        while (ln > 0);
    }

    return 0;
}

/* dump membuf contents into file pointed by descriptor 'fd' and close this
 membuf, deallocating memory */
int membuf_write (int mbid, int fd)
{
    int i;

    if (!MB_init || mbid < 0 || mbid >= MB_N || !MB[mbid].opened) return -1;

    for (i=0; i<MB[mbid].nbufs; i++)
    {
        if (i != MB[mbid].nbufs-1)
        {
            xwrite (fd, MB[mbid].bufs[i], MB[mbid].bufsize);
        }
        else
        {
            xwrite (fd, MB[mbid].bufs[i], MB[mbid].bufsize-MB[mbid].left);
        }
    }

    return 0;
}

/* printf-like function for membuf_* interface */
int membuf_printf (int mbid, char *fmt, ...)
{
    int      nb;
    va_list  args;
    char     buf[8192];

    if (!MB_init || mbid < 0 || mbid >= MB_N || !MB[mbid].opened) return -1;

    va_start (args, fmt);
    nb = vsnprintf (buf, sizeof (buf), fmt, args);
    if (nb >= sizeof(buf))
    {
        char *p = xmalloc (nb+1);
        vsnprintf (buf, sizeof (buf), fmt, args);
        membuf_put (mbid, p);
        free (p);
    }
    else
    {
        membuf_put (mbid, buf);
    }
    va_end (args);

    return 0;
}

/* gather all buffers into one big malloc()ed string */
char *membuf_accumulate (int mbid)
{
    int  i, nb, size;
    char *s;

    if (!MB_init || mbid < 0 || mbid >= MB_N || !MB[mbid].opened) return NULL;

    nb = MB[mbid].bufsize * (MB[mbid].nbufs-1) + MB[mbid].bufsize-MB[mbid].left;
    s = malloc (nb+1);
    if (s == NULL) return NULL;

    nb = 0;
    for (i=0; i<MB[mbid].nbufs; i++)
    {
        size = (i == MB[mbid].nbufs-1 ?
                MB[mbid].bufsize-MB[mbid].left : MB[mbid].bufsize);
        memcpy (s+nb, MB[mbid].bufs[i], size);
        nb += size;
    }
    s[nb] = '\0';

    return s;
}

