#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* extended version of mkdir. creates all intermediate directories if needed */
int make_subtree (char *s)
{
    char   *p, *q;
    int    rc;

    /* skip things like "d:" */
    if (strlen (s) == 2 && s[1] == ':') return 0;
    
    q = strdup (s);
    str_translate (q, '\\', '/');

    /* break path into directories and check each one */
    p = q;
    if (*p == '/') p++;
    rc = 0;
    while (1)
    {
        /* separate next directory */
        p = strchr (p, '/');
        if (p != NULL) *p = '\0';

        if (q[1] != ':' || p != q+2)
        {
            /* check whether dir exists and can we create one */
#ifdef __MINGW32__
            if (access (q, F_OK) != 0 && mkdir (q))
#else
            if (access (q, F_OK) != 0 && mkdir (q, 0777))
#endif
            {
                rc = -1;
                break;
            }
        }

        /* put slash back into string and move on */
        if (p != NULL) *p = '/';     
        if (p == NULL) break;
        p++;
    }
    
    free (q);
    return rc;
}

/* loads file into malloc()ed buffer. returns length of the file
 or -1 if error occurs */
long load_file (char *filename, char **p)
{
    int fh;
    unsigned long l;

    /* open file */
    fh = open (filename, O_RDONLY|BINMODE);
    if (fh < 0) return -1;
    
    /* allocate buffer */
    l = file_length (fh);
    if (l <= 0) {close (fh); return l;}
    *p = xmalloc (l+2);

    /* read file into buffer */
    if (read (fh, *p, l) != l) {free (*p); close (fh); return -1;}
    (*p)[l] = '\0';
    
    close (fh);
    return l;
}

#define STDIN_BUFFER 16384

/* reads stdin into malloc()ed buffer. returns length of the buffer
 or -1 if error occurs */
int load_stdin (char **p)
{
    int  bl, bl_a, n;
    char *buffer, *p1;

    /* preallocate buffer */
    bl_a = STDIN_BUFFER;
    bl   = 0;
    buffer = malloc (bl_a);
    if (buffer == NULL) return -1;

    /* read file into buffer */
    while (TRUE)
    {
        n = read (fileno(stdin), buffer+bl, bl_a-bl);
        if (n <= 0) break;
        bl += n;
        if (bl_a-bl < STDIN_BUFFER)
        {
            bl_a *= 2;
            p1 = realloc (buffer, bl_a);
            if (p1 == NULL)
            {
                free (buffer);
                return -1;
            }
            buffer = p1;
        }
    }

    /* try to shrink buffer to avoid wasting memory */
    p1 = realloc (buffer, bl+1);
    if (p1 != NULL) buffer = p1;

    buffer[bl] = '\0';
    *p = buffer;
    return bl;
}

/* reads file and treates it as text: breaks into individual lines,
 returns pointer to these lines and number of them.
 to free memory, issue free() on first element of pointers array
 and then free pointers array:
 char  **str;
 if (load_textfile ("information.txt", &str) < 0) return;
 ...
 free (str[0]);
 free (str);
 return value: number of lines in the file, or -1 if error */

#define NEWLINE    0x0A
#define CARRIAGE   0x0D

int load_textfile (char *filename,  char ***lines)
{
   int            i;
   char           *buffer;
   long           len;

   /* read file into memory */
   len = load_file (filename, &buffer);
   if (len <= 0) return -1;

   /* replace all NULLs with spaces */
   for (i=0; i<len; i++)
       if (buffer[i] == '\0') buffer[i] = ' ';

   return text2lines (buffer, lines);
   /*
   len = str_refine_buffer (buffer);

   p1 = buffer;
   numlines = 0;
   while (p1-buffer < len)
   {
       if (*p1 == '\0') numlines++;
       p1++;
   }

   *lines = malloc (sizeof(void *) * (numlines+3));

   p1 = buffer;
   for (i=0; i<numlines; i++)
   {
       (*lines)[i] = p1;
       while (*p1 != '\0') p1++;
       p1++;
   }


   numlines = 0;
   na = 64;
   *lines = xmalloc (na * sizeof (char *));
   p = buffer;
   (*lines)[numlines++] = buffer;
   while (*p)
   {
       if (*p == '\n' || *p == '\r')
       {
           *p = '\0';
           p++;
           while (*p && (*p == '\n' || *p == '\r')) p++;
           if (*p)
           {
               if (numlines == na)
               {
                   na *= 2;
                   *lines = xrealloc (*lines, na * sizeof (char *));
               }
               (*lines)[numlines++] = p;
           }
       }
       else
           p++;
   }

   return numlines;
   */
}

/* breaks text into lines. original text is not preserved,
 'lines' array is malloc()ed and its individual entries point
 to places in 's' */
int text2lines (char *s, char ***lines)
{
    int numlines, na;
    char *p;

    numlines = 0;
    na = 64;
    *lines = xmalloc (na * sizeof (char *));
    p = s;
    (*lines)[numlines++] = p;
    while (*p)
    {
        if (*p == '\n' || *p == '\r')
        {
            *p = '\0';
            p++;
            while (*p && (*p == '\n' || *p == '\r')) p++;
            if (*p)
            {
                if (numlines == na)
                {
                    na *= 2;
                    *lines = xrealloc (*lines, na * sizeof (char *));
                }
                (*lines)[numlines++] = p;
            }
        }
        else
            p++;
    }

    return numlines;
}

int flongprint (FILE *fp, char *s, int maxlen)
{
    int i, len = strlen (s);

    if (len == 0) return 0;
    if (maxlen < 3) return -1;

    maxlen -= 2;
    for (i=0; i<len/maxlen+1; i++)
    {
        if ((i+1)*maxlen >= len)
        {
            fwrite (s+i*maxlen, 1, len-i*maxlen, fp);
        }
        else
        {
            fwrite (s+i*maxlen, 1, maxlen, fp);
            fputs ("\\\n", fp);
        }
    }
    return 0;
}

int longprint (int fd, char *s, int maxlen)
{
    int i, len = strlen (s);

    if (len == 0) return 0;
    if (maxlen < 3) return -1;

    maxlen -= 2;
    for (i=0; i<len/maxlen+1; i++)
    {
        if ((i+1)*maxlen >= len)
        {
            write (fd, s+i*maxlen, len-i*maxlen);
        }
        else
        {
            write (fd, s+i*maxlen, maxlen);
            write (fd, "\\\n", 2);
        }
    }
    return 0;

}

/* prints file on stdout */
void print_file (char *filename)
{
    FILE  *fp;
    char  buf1[8192];
    
    /* read file line by line and print it */
    fp = fopen (filename, "rt");
    if (fp == NULL) return;
    while (fgets (buf1, sizeof(buf1), fp) != NULL) fputs (buf1, stdout);
    fclose (fp);
}

/* some compilers mysteriously lack filelength(). this is a replacement */
unsigned long file_length (int fd)
{
    unsigned long ln1, ln2;
    ln1 = lseek (fd, 0, SEEK_CUR);
    ln2 = lseek (fd, 0, SEEK_END);
    lseek (fd, ln1, SEEK_SET);
    return ln2;
}

#define DEFAULT_PROBE_SIZE 1024

static char *allowed = "\n\r\t\b\26";

/* tries to determine whether file is text (not binary)
 file. set probe_size to -1 to use default. returns 0
 if file is binary, 1 if file is text, and -1 on error
 (file does not exist or has zero length) */
int is_textfile (char *filename, int probe_size)
{
    int   i, fh, n;
    unsigned char  *buf;
    long  l;

    if (probe_size < 0) probe_size = DEFAULT_PROBE_SIZE;

    /* try to open file */
    fh = open (filename, O_RDONLY|BINMODE);
    if (fh < 0) return -1;

    /* check its length */
    l = file_length (fh);
    if (l <= 0)
    {
        close (fh);
        return -1;
    }
    probe_size = min1 (l, probe_size);

    /* read a piece from the file */
    buf = xmalloc (probe_size);
    n = read (fh, buf, probe_size);
    if (n != probe_size)
    {
        /* error reading file; perhaps insufficient privileges? */
        close (fh);
        free (buf);
        return -1;
    }
    close (fh);

    /* now we've got the fragment of the file */
    for (i=0; i<n; i++)
    {
        if (buf[i] < 32 && strchr (allowed, buf[i]) == NULL)
        {
            free (buf);
            return FALSE;
        }
    }

    free (buf);
    return TRUE;
}

/* writes buffer into file truncating existing one if necessary.
 returns bytes written or -1 if error occurs */
long dump_file (char *filename, char *p, long length)
{
    int fh;
    unsigned long l;

    /* open file */
    fh = open (filename, O_WRONLY|O_CREAT|O_TRUNC|BINMODE, 0777);
    if (fh < 0) return -1;

    /* write buffer into file */
    l = write (fh, p, length);
    
    close (fh);
    return l;
}

/* copies file from dest to src. avoid large files. returns 0 on
 success, 1 on failure */
int copy_file (char *src, char *dest)
{
    long  l;
    char  *p;
    
    l = load_file (src, &p);
    if (l < 0) return 1;
    if (dump_file (dest, p, l) != l) return 1;

    return 0;
}

/* tries to guess whether file is an index file containing descriptions */
int isindexfile (char *name)
{
    if (!stricmp1 (name, "00index.txt")) return TRUE;
    if (!stricmp1 (name, "00_index.txt")) return TRUE;
    if (!stricmp1 (name, "00index")) return TRUE;
    if (!stricmp1 (name, "00-index")) return TRUE;
    if (!stricmp1 (name, "index.txt")) return TRUE;
    if (!stricmp1 (name, "files.bbs")) return TRUE;
    if (!stricmp1 (name, "descript.ion")) return TRUE;
    if (!stricmp1 (name, "index")) return TRUE;
    if (!stricmp1 (name, "!index")) return TRUE;
    return FALSE;
}

#if HAVE_FLOCK
/* opens file getting a lock on it. returns -1 if open
 fails or file descriptor on success. */
int openlock (char *name, int flags, int mode)
{
    int rc, fd;

    /* somehow Linux does not have O_EXLOCK flag */
    fd = open (name, flags, mode);
    if (fd < 0) return fd;
    rc = flock (fd, LOCK_EX);
    if (rc < 0)
    {
        close (fd);
        return rc;
    }
    return fd;
}
#endif

/* copy 'nb' bytes from 'in' to 'out', using 'mem' bytes as temp buffer.
 if 'buf' is not NULL, it is used as temp buffer containing 'mem' bytes.
 returns number of bytes copied */
int64_t copy_bytes (FILE *in, FILE *out, int64_t nb, int mem, char *buf)
{
    int64_t copied;
    int     to_copy;
    char *p;

    if (buf != NULL) p = buf;
    else             p = xmalloc (mem);

    copied = 0;
    while (TRUE)
    {
        to_copy = min1 (mem, nb);
        if (fread (p, 1, to_copy, in) != to_copy)
            error1 ("failed to read from stream %d bytes\n", to_copy);
        if (fwrite (p, 1, to_copy, out) != to_copy)
            error1 ("failed to write to stream %d bytes\n", to_copy);
        copied += to_copy;
        if (copied == nb) break;
    }

    if (buf == NULL) free (p);

    return copied;
}

/****************************************************************/

int64_t f_length (char *fname)
{
    struct stat st;
    int rc;

    rc = stat (fname, &st);
    if (rc < 0) return (int64_t)-1;
    return st.st_size;
}

