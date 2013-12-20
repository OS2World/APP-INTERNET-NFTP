#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int clock1_call_counter = 0;

/* second counter, as a double for easy operation */
double clock1 (void)
{
#ifndef __MINGW32__
    struct timeval  tv;
    gettimeofday (&tv, NULL);

    clock1_call_counter++;
    return ((double)tv.tv_sec) + ((double)tv.tv_usec)/1000000.;
#else
    clock1_call_counter++;
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

void sleep1 (double intr)
{
    double t = clock1() + intr;

    while (TRUE)
    {
        if (clock1() > t) break;
    }
}

/* prints error message to stderr and exits */
void error1 (char *format, ...)
{
   va_list       args;

   va_start (args, format);
   vfprintf (stderr, format, args);
   va_end (args);

   exit (1);
}

/* prints error message to fp and exits */
void error2 (FILE *fp, char *format, ...)
{
   va_list       args;

   va_start (args, format);
   vfprintf (fp, format, args);
   va_end (args);

   exit (1);
}

/* prints error message to fp and stderr and exits */
void error3 (FILE *fp, char *format, ...)
{
   va_list       args;

   va_start (args, format);
   vfprintf (stderr, format, args);
   vfprintf (fp, format, args);
   va_end (args);

   exit (1);
}

/* prints warning message to stderr */
void warning1 (char *format, ...)
{
   va_list       args;

   va_start (args, format);
   vfprintf (stderr, format, args);
   va_end (args);
}

/* prints warning message to fp */
void warning2 (FILE *fp, char *format, ...)
{
   va_list       args;

   if (fp == NULL) return;

   va_start (args, format);
   vfprintf (fp, format, args);
   va_end (args);
   fflush (fp);
}

/* prints warning message to fp and stderr */
void warning3 (FILE *fp, char *format, ...)
{
   va_list       args;

   if (fp == NULL) return;

   va_start (args, format);
   vfprintf (stderr, format, args);
   vfprintf (fp, format, args);
   va_end (args);

   fflush (fp);
}

FILE *tools_debug = NULL;

/* internal debug output routine */
void debug_tools (char *format, ...)
{
    va_list       args;
    
    if (tools_debug == NULL) return;
    
    va_start (args, format);
    vfprintf (tools_debug, format, args);
    fflush (tools_debug);
    va_end (args);
}

/* a version of snprintf. uses native snprintf when available and surrogate
 when not */
int snprintf1 (char *str, size_t size, const char *format, ...)
{
    va_list  args;
    int      nb;

    va_start (args, format);
    nb = vsnprintf1 (str, size, format, args);
    va_end (args);

    return nb;
}

int vsnprintf1 (char *str, size_t size, const char *format, va_list args)
{
    /* we assume (maybe somewhat naive) that if system has snprintf it also
     has vsnprintf */
#if HAVE_VSNPRINTF
    return vsnprintf (str, size, format, args);
#else
    char     *buf;
    int      n, n1;

    n1 = max1 (1024*1024, size+1);
    buf = xmalloc (n1);
    n = vsprintf (buf, format, args);

    if (n >= n1)
        error1 ("snprintf1() buffer overflow! format=%s\n", format);
    memcpy (str, buf, min1 (n, size));
    str[min1 (n, size)] = '\0';
    free (buf);

    return n;
#endif
}

/* converts file permissions from textual form (drwxr-xr--) to binary form as used
 in e.g. stat() call. string must be at least 10 symbols long. when error occurs
 0 is returned */
int perm_t2b (char *s)
{
    int rights = 0;
    
    if (strlen (s) < 10) return 0;

    /* entry type */
    switch (s[0])
    {
    /*case '?': rights |= S_IFSOCK; break;*/
    case 'l': rights |= S_IFLNK; break;
    case '-': rights |= S_IFREG; break;
    case 'b': rights |= S_IFBLK; break;
    case 'd': rights |= S_IFDIR; break;
    case 'c': rights |= S_IFCHR; break;
    default:  return 0;
    /*case '?': rights |= S_IFIFO; break;*/
    }

    /* owner read */
    switch (s[1])
    {
    case 'r': rights |= S_IRUSR; break;
    case '-': break;
    default:  return 0;
    }
    
    /* owner write */
    switch (s[2])
    {
    case 'w': rights |= S_IWUSR; break;
    case '-': break;
    default:  return 0;
    }
    
    /* owner execute */
    switch (s[3])
    {
    case 'x': rights |= S_IXUSR; break;
    case 's': rights |= S_IXUSR|S_ISUID; break;
    case 'S': rights |= S_ISUID; break;
    case '-': break;
    default:  return 0;
    }
    
    /* group read */
    switch (s[4])
    {
    case 'r': rights |= S_IRGRP; break;
    case '-': break;
    default:  return 0;
    }
    
    /* group write */
    switch (s[5])
    {
    case 'w': rights |= S_IWGRP; break;
    case '-': break;
    default:  return 0;
    }
    
    /* group execute */
    switch (s[6])
    {
    case 'x': rights |= S_IXGRP; break;
    case 's': rights |= S_IXGRP|S_ISGID; break;
    case 'S': rights |= S_ISGID; break;
    case '-': break;
    default:  return 0;
    }
    
    /* others read */
    switch (s[7])
    {
    case 'r': rights |= S_IROTH; break;
    case '-': break;
    default:  return 0;
    }
    
    /* others write */
    switch (s[8])
    {
    case 'w': rights |= S_IWOTH; break;
    case '-': break;
    default:  return 0;
    }
    
    /* others execute */
    switch (s[9])
    {
    case 'x': rights |= S_IXOTH; break;
    case 'T': break;                    /* ignore sticky bit */
    case 't': rights |= S_IXOTH; break; /* ignore sticky bit */
    case '-': break;
    default:  return 0;
    }
    
    return rights;
}

/* converts file permissions from binary form to textual (rwxr-xr--).
 returns pointer to static, NULL-terminated buffer of 9 bytes long buffer */
char *perm_b2t (int rights)
{
    static char perm[10];

    strcpy (perm, "---------");
    
    if (rights & S_IRUSR) perm[0] = 'r';
    if (rights & S_IWUSR) perm[1] = 'w';
    if (rights & S_IXUSR) perm[2] = 'x';
    if (rights & S_ISUID) perm[2] = 's';
    
    if (rights & S_IRGRP) perm[3] = 'r';
    if (rights & S_IWGRP) perm[4] = 'w';
    if (rights & S_IXGRP) perm[5] = 'x';
    if (rights & S_ISGID) perm[5] = 's';
    
    if (rights & S_IROTH) perm[6] = 'r';
    if (rights & S_IWOTH) perm[7] = 'w';
    if (rights & S_IXOTH) perm[8] = 'x';

    return perm;
}

#define MAX_BUFS 8

static char buffers[MAX_BUFS][27];
static int  ib=0;

/* pretty-printing for the whole number. returns pointer to statically
 allocated, NULL-terminated buffer of length 13.
 4,000,000,000
 */
char *insert_commas (unsigned long number)
{
    char  *buf, *p;

    buf = buffers[ib];
    ib = (ib == MAX_BUFS-1) ? 0 : ib+1;

    buf[0]  = (number / 1000000000) % 10 + '0';
    buf[1]  = ',';
    buf[2]  = (number / 100000000 ) % 10 + '0';
    buf[3]  = (number / 10000000  ) % 10 + '0';
    buf[4]  = (number / 1000000   ) % 10 + '0';
    buf[5]  = ',';
    buf[6]  = (number / 100000    ) % 10 + '0';
    buf[7]  = (number / 10000     ) % 10 + '0';
    buf[8]  = (number / 1000      ) % 10 + '0';
    buf[9]  = ',';
    buf[10] = (number / 100       ) % 10 + '0';
    buf[11] = (number / 10        ) % 10 + '0';
    buf[12] = (number             ) % 10 + '0';
    buf[13] = '\0';

    p = buf;
    while (*p && (*p == '0' || *p == ','))
        *(p++) = ' ';
    
    if (buf[12] == ' ') buf[12] = '0';
    
    return buf;
}

/* pretty-printing for the whole number. returns pointer to statically
 allocated, NULL-terminated buffer. Unlike insert_commas, this version
 does not pad front with spaces */
char *insert_commas2 (unsigned long number)
{
    char *p;

    p = insert_commas (number);
    while (*p && *p == ' ') p++;
    
    return p;
}

/* similar to insert_commas but using int64 input. static buffer
 is 26 bytes long.
 16,000,000,000,000,000,000
 */
char *insert_commas3 (int64_t number)
{
    u_int64_t n;
    char      *buf, *p;

    buf = buffers[ib];
    ib = (ib == MAX_BUFS-1) ? 0 : ib+1;

    n = number;
    buf[0]  = (n / 10000000000000000000uLL ) % 10 + '0';
    buf[1]  = (n / 1000000000000000000uLL ) % 10 + '0';  buf[2]  = ',';
    buf[3]  = (n / 100000000000000000uLL ) % 10 + '0';
    buf[4]  = (n / 10000000000000000uLL ) % 10 + '0';
    buf[5]  = (n / 1000000000000000uLL ) % 10 + '0'; buf[6]  = ',';
    buf[7]  = (n / 100000000000000uLL ) % 10 + '0';
    buf[8]  = (n / 10000000000000uLL ) % 10 + '0';
    buf[9]  = (n / 1000000000000uLL ) % 10 + '0'; buf[10] = ',';
    buf[11] = (n / 100000000000uLL ) % 10 + '0';
    buf[12] = (n / 10000000000uLL ) % 10 + '0';
    buf[13] = (n / 1000000000uLL ) % 10 + '0'; buf[14] = ',';
    buf[15] = (n / 100000000uLL ) % 10 + '0';
    buf[16] = (n / 10000000uLL ) % 10 + '0';
    buf[17] = (n / 1000000uLL ) % 10 + '0'; buf[18] = ',';
    buf[19] = (n / 100000uLL ) % 10 + '0';
    buf[20] = (n / 10000uLL ) % 10 + '0';
    buf[21] = (n / 1000uLL ) % 10 + '0'; buf[22] = ',';
    buf[23] = (n / 100uLL ) % 10 + '0';
    buf[24] = (n / 10uLL ) % 10 + '0';
    buf[25] = (n        ) % 10 + '0';
    buf[26] = '\0';

    p = buf;
    while (*p && (*p == '0' || *p == ','))
        *(p++) = ' ';
    
    if (buf[25] == ' ') buf[25] = '0';
    
    return buf;

}

/* similar to insert_commas2 but for 64bit integers */
char *insert_commas4 (int64_t number)
{
    char *p;

    p = insert_commas3 (number);
    while (*p && *p == ' ') p++;
    
    return p;
}

/* this one does not really inserts commas but just produces
 a string with 64-bit integer. leading spaces are skipped */
char *insert_commas5 (int64_t number)
{
    u_int64_t n;
    char      *buf, *p;

    buf = buffers[ib];
    ib = (ib == MAX_BUFS-1) ? 0 : ib+1;

    n = number;
    buf[0]  = (n / 10000000000000000000uLL ) % 10 + '0';
    buf[1]  = (n / 1000000000000000000uLL ) % 10 + '0';
    buf[2]  = (n / 100000000000000000uLL ) % 10 + '0';
    buf[3]  = (n / 10000000000000000uLL ) % 10 + '0';
    buf[4]  = (n / 1000000000000000uLL ) % 10 + '0';
    buf[5]  = (n / 100000000000000uLL ) % 10 + '0';
    buf[6]  = (n / 10000000000000uLL ) % 10 + '0';
    buf[7]  = (n / 1000000000000uLL ) % 10 + '0';
    buf[8]  = (n / 100000000000uLL ) % 10 + '0';
    buf[9]  = (n / 10000000000uLL ) % 10 + '0';
    buf[10] = (n / 1000000000uLL ) % 10 + '0';
    buf[11] = (n / 100000000uLL ) % 10 + '0';
    buf[12] = (n / 10000000uLL ) % 10 + '0';
    buf[13] = (n / 1000000uLL ) % 10 + '0';
    buf[14] = (n / 100000uLL ) % 10 + '0';
    buf[15] = (n / 10000uLL ) % 10 + '0';
    buf[16] = (n / 1000uLL ) % 10 + '0';
    buf[17] = (n / 100uLL ) % 10 + '0';
    buf[18] = (n / 10uLL ) % 10 + '0';
    buf[19] = (n        ) % 10 + '0';
    buf[20] = '\0';

    p = buf;
    while (*p && (*p == '0' || *p == ','))
        *(p++) = ' ';
    
    if (buf[19] == ' ') buf[19] = '0';
    
    return buf;

}

/* skips leading zeroes on 64-bit number */
char *insert_commas6 (int64_t number)
{
    char *p;

    p = insert_commas5 (number);
    while (*p && *p == ' ') p++;
    
    return p;
}

/* very fast malloc without overhead and free(). makes a copy of string
 `s' and returns pointer to newly allocated place */
#define  CHUNK_SIZE   256*1024

static  char *chunk = NULL, *fchunk;

char *chunk_add (char *s)
{
    int   len = strlen (s);
    char  *p;
    
    if (chunk == NULL || chunk+CHUNK_SIZE-fchunk < len+3)
    {
        chunk = malloc (max1(CHUNK_SIZE, len+2));
        fchunk = chunk;
    }

    p = fchunk;
    strcpy (p, s);
    fchunk += len+1;
    
    return p;
}

/* returns 1 when 's' is correct Unix listing entry. check is
 rather primitive but discards most cases */
int is_unix_file_entry (char *s)
{
    if (strlen (s) < 10) return FALSE;
    if (s[10] != ' ') return FALSE;
    if (perm_t2b (s) == 0) return FALSE;
    return TRUE;
}

/* these are allowed: Ctrl-Z (eof), BackSpace */
/*static char *allowed_ascii = "\x1a\x08";*/

/* returns concentration of binary chars in the
 input string 's' of length 'nbytes'. in other words
 when '0.01' is returned then 1% of the input is binary. */
float binchars (char *s, int nbytes)
{
    int i, n, badchars;

    /* allow NULs and Ctrl-Zs at the end of the buffer
     (pretty frequent situation) */
    while (nbytes > 0 && (s[nbytes-1] == '\0' || s[nbytes-1] == '\x1a'))
           nbytes--;

    /* if string is 0 bytes long we assume it contains only
     binary characters. really this does not mean anything. */
    if (nbytes == 0) return 1.;

    /* size of the part being analyzed. */
    /*n = min1 (nbytes, 1024*1024);*/
    n = nbytes;
    
    /* count chars which are 'binary'. isspace() catches
     some non-printable chars like \n, \r, \t. */
    badchars = 0;
    for (i=0; i<n; i++)
    {
        if (s[i] == '\0' ||
            ((unsigned char)s[i] < 127 &&
             !isprint((unsigned char)s[i]) &&
             !isspace((unsigned char)s[i]))
            )
        {
            badchars++;
        }
    }

    return (float)badchars / (float)n;
}

/* returns concentration of nontext chars in the
 input string 's' of length 'nbytes'. in other words
 when '0.01' is returned then 1% of the input is not a text (i.e.
 numbers, punctuation etc.) 'Text chars' are: A-Z, symbols from high half
 of ASCII table. Space characters are not 'text chars' and excluded from
 the byte count (this is to keep files with large amount of spaces inside) */
float nontextchars (char *s, int nbytes)
{
    int i, n, badchars, spaces;
    unsigned char *us;

    /* allow NULs and Ctrl-Zs at the end of the buffer
     (pretty frequent situation) */
    while (nbytes > 0 && (s[nbytes-1] == '\0' || s[nbytes-1] == '\x1a'))
           nbytes--;

    /* if string is 0 bytes long we assume it only contains nontext
     characters (no meaningful content) */
    if (nbytes == 0) return 1.0;

    /* size of the part being analyzed. */
    /*n = min1 (nbytes, 1024*1024);*/
    n = nbytes;
    
    /* count nontext chars and spaces (separately) */
    badchars = 0;
    spaces = 0;
    us = (unsigned char *)s;
    for (i=0; i<n; i++)
    {
        if (!((us[i] >= 'a' && us[i] <= 'z') ||
              (us[i] >= 'A' && us[i] <= 'A') ||
              us[i] >= 128))
        {
            if (isspace (us[i])) spaces++;
            else                 badchars++;
        }
    }

    /* return 'all chars are nontext' if text is made of spaces */
    if (n == spaces) return 1.0;

    return (float)badchars / (float)(n-spaces);
}

/* prints binary input to stdout, replacing non-printable chars
 with [0x12] equivalents */
void print_binary (char *s, int len)
{
    int i;

    for (i=0; i<len; i++)
    {
        if ((unsigned char)s[i] >= ' ' &&
            (
             isalnum ((unsigned char)s[i]) ||
             ispunct ((unsigned char)s[i])
            ))
        {
            fputc (s[i], stdout);
        }
        else
        {
            printf ("[%02x]", (unsigned char)s[i]);
        }
    }
}

/* prints bytes bit-by-bit: 0000-0000 */
void fprint_bits (FILE *fp, char *s, int len)
{
    int i, j;
    char buffer[11];

    for (i=0; i<len; i++)
    {
        memset (buffer, '0', 9);
        buffer[4] = '-';
        for (j=0; j<4; j++)
        {
            if (s[i] & (1<<j)) buffer[8-j] = '1';
        }
        for (j=4; j<8; j++)
        {
            if (s[i] & (1<<j)) buffer[7-j] = '1';
        }
        if (i != len-1) strcpy (buffer+9, " ");
        else            buffer[9] = '\0';
        fprintf (fp, buffer);
    }
}

/* ------------------------------------------------------------------- */

int cmp_range (const void *e1, const void *e2)
{
    id_range_t *ir1, *ir2;

    ir1 = (id_range_t *) e1;
    ir2 = (id_range_t *) e2;

    return ir1->s_id - ir2->s_id;
}

/* performs intersection of two range lists: 'out' contains only
 overlapping portions of lists */
int ranges_intersect (rangeset_t *in1, rangeset_t *in2, rangeset_t *out)
{
    int nr, i, j, nr_a, rc, r, l, right, left, nr1, nr2;
    id_range_t ir3, *ir, rn1, rn2, *ir1, *ir2, *irtmp;

    if (in1->nr == 0 || in2->nr == 0)
    {
        out->nr = 0;
        out->ir = NULL;
        return 0;
    }

    /* make sure that list1 is shorter than list2 */
    if (in1->nr > in2->nr)
    {
        nr1 = in2->nr; ir1 = in2->ir;
        nr2 = in1->nr; ir2 = in1->ir;
    }
    else
    {
        nr1 = in1->nr; ir1 = in1->ir;
        nr2 = in2->nr; ir2 = in2->ir;
    }

    /*warning1 ("intersecting two range lists:\n");
      warning1 ("list 1 (%d items):\n", nr1);
      for (i=0; i<nr1; i++)
        warning1 ("%d:%d ", ir1[i].s_id, ir1[i].e_id);
      warning1 ("\n");
      warning1 ("list 2 (%d items):\n", nr2);
      for (i=0; i<nr2; i++)
        warning1 ("%d:%d ", ir2[i].s_id, ir2[i].e_id);
      warning1 ("\n");
    */

    /* preallocate space for result */
    nr_a   = max1 (nr1, nr2);
    nr     = 0;
    ir     = malloc (sizeof(id_range_t) * nr_a);
    if (ir == NULL) return -1;

    /* how we do it:
     one of the lists is shorter than the other. let's suppose than list1
     is shorter. then we use binary search (bracket()) to locate ranges
     close to the starting value of the */
    for (i=0; i<nr1; i++)
    {
        /* locate ranges from list2 which can have intersections with i-th
         range from list1 */
        ir3.s_id = ir1[i].s_id;
        rc = bracket (&ir3, ir2, nr2, sizeof(id_range_t), cmp_range, &l, &r);
        switch (rc)
        {
        case 0: left = l; break;
        case 1: left = l; break;
        case -1: left = 0; break;
        case -2: left = nr2-1; break;
        default: continue;
        }

        ir3.s_id = ir1[i].e_id;
        rc = bracket (&ir3, ir2, nr2, sizeof(id_range_t), cmp_range, &l, &r);
        switch (rc)
        {
        case 0: right = max1 (r-1, 0); break;
        case 1: right = r; break;
        case -1: continue;
        case -2: right = nr2-1; break;
        default: continue;
        }

        /* check validity of the left/right boundaries */
        if (left > right)
        {
            warning1 ("warning: left (%d) > right (%d)\n", left, right);
            continue;
        }

        /*warning1 ("range %d:%d may concern ", ir1[i].s_id, ir1[i].e_id);
          for (j=left; j<=right; j++)
            warning1 ("%d:%d ", ir2[j].s_id, ir2[j].e_id);
          warning1 ("\n");
        */

        /* try to intersect individual ranges compiling new range list */
        for (j=left; j<=right; j++)
        {
            /* rn1 has smaller left boundary than rn2 */
            if (ir1[i].s_id > ir2[j].s_id)
            {
                rn1 = ir2[j];
                rn2 = ir1[i];
            }
            else
            {
                rn1 = ir1[i];
                rn2 = ir2[j];
            }
            /* no intersection at all */
            if (rn1.e_id < rn2.s_id) continue;
            /* partial intersection */
            if (rn1.e_id < rn2.e_id)
            {
                if (nr == nr_a)
                {
                    nr_a *= 2;
                    irtmp = realloc (ir, sizeof(id_range_t) * nr_a);
                    if (irtmp == NULL)
                    {
                        free (ir);
                        return -1;
                    }
                    ir = irtmp;
                }
                ir[nr].s_id = rn2.s_id;
                ir[nr].e_id = rn1.e_id;
                nr++;
            }
            /* rn2 is inside rn1 */
            if (rn1.e_id >= rn2.e_id)
            {
                if (nr == nr_a)
                {
                    nr_a *= 2;
                    irtmp = realloc (ir, sizeof(id_range_t) * nr_a);
                    if (irtmp == NULL)
                    {
                        free (ir);
                        return -1;
                    }
                    ir = irtmp;
                }
                ir[nr++] = rn2;
            }
        }
    }

    if (nr == 0)
    {
        free (ir);
        out->nr = 0;
        out->ir = NULL;
        return 0;
    }

    /* free unused memory (shrink ir) */
    irtmp = realloc (ir, sizeof(id_range_t) * nr);
    if (irtmp == NULL)
    {
        free (ir);
        return -1;
    }
    ir = irtmp;

    out->ir = ir;
    out->nr = nr;
    return 0;

}

/* performs merge of two range lists: 'out' contains ranges combined
 from both lists */
int ranges_merge (rangeset_t *in1, rangeset_t *in2, rangeset_t *out)
{
    int i, j;

    if (in1->nr + in2->nr == 0)
    {
        out->nr = 0;
        out->ir = NULL;
        return 0;
    }

    /* allocate space for output */
    out->ir = malloc (sizeof(id_range_t) * (in1->nr + in2->nr));
    if (out->ir == NULL) return -1;

    /* copy ranges from both inputs to output (we need one sorted
     list of ranges to perform merge) */
    memcpy (out->ir,         in1->ir, sizeof(id_range_t) * in1->nr);
    memcpy (out->ir+in1->nr, in2->ir, sizeof(id_range_t) * in2->nr);

    /* sort ranges before merging */
    qsort (out->ir, in1->nr + in2->nr, sizeof(id_range_t), cmp_range);

    for (i=1, j=0; i<in1->nr+in2->nr; i++)
    {
        if (out->ir[i].s_id <= out->ir[j].e_id+1)
        {
            if (out->ir[i].e_id > out->ir[j].e_id)
                out->ir[j].e_id = out->ir[i].e_id;
        }
        else
        {
            out->ir[++j] = out->ir[i];
        }
    }
    out->nr = j+1;

    return 0;
}

/* ------------------------------------------------------------------- */

int merge_ranges (int nr, id_range_t *ir)
{
    int i, j;

    if (nr == 0) return 0;

    for (i=1, j=0; i<nr; i++)
    {
        if (ir[i].s_id <= ir[j].e_id+1)
        {
            if (ir[i].e_id > ir[j].e_id)
                ir[j].e_id = ir[i].e_id;
        }
        else
        {
            ir[++j] = ir[i];
        }
    }

    return j+1;
}

/* 'x' in 'pwr' power */
double ipow (double x, int pwr)
{
    double r;
    int    i;

    r = 1.0;
    for (i=0; i<pwr; i++) r *= x;

    return r;
}

/* parses command-line argument representing file and column 'file:column'
 and reads corresponding column from corresponding file */
int read_one_column (char *arg, double **data)
{
    char *p, *s;
    int  nrec, ncol;

    s = strdup (arg);
    p = strchr (s, ':');
    if (p == NULL) goto invalid_format;
    *p = '\0';
    if (strlen (s) == 0 || strlen (p+1) == 0) goto invalid_format;
    ncol = atoi (p+1);

    nrec = read_column (s, ncol, data);

    free (s);
    return nrec;

invalid_format:
    warning1 ("invalid file:column specification '%s'\n", arg);
    free (s);
    return -1;
}

/* read two column argument (filename:x:y) */
int read_two_columns (char *arg, double **data1, double **data2)
{
    int    nrec, nc;
    double **dp;

    nrec = read_n_columns (arg, &nc, &dp);
    if (nc != 2) error1 ("number of columns in '%s' is not 2\n", arg);

    *data1 = dp[0];
    *data2 = dp[1];
    return nrec;
}

/* parses command-line argument representing file and column 'file:column'
 and reads corresponding column from corresponding file */
int read_n_columns (char *arg, int *n, double ***data)
{
    char **L;
    int  i, nr, nrec, ncol;
    double *dp;

    *n = str_parseline (arg, &L, ':');
    if (*n <= 1) goto invalid_format;
    (*n)--;

    *data = xmalloc (sizeof (double *) * (*n));

    nrec = -1;
    for (i=0; i<(*n); i++)
    {
        ncol = atoi (L[i+1]);
        nr = read_column (L[0], ncol, &dp);
        if (i > 1 && nr != nrec)
            error1 ("different number of rows for different columns\n");
        nrec = nr;
        (*data)[i] = dp;
        free (L[i+1]);
    }
    free (L[0]);
    free (L);

    return nrec;

invalid_format:
    warning1 ("invalid file:column1[:column2[:...]] specification '%s'\n", arg);
    free (L);
    return -1;
}

/* loads a column from data file. returns number of elements in the 'data'
 array which is malloc()ed. returns -1 when data file cannot be loaded,
 -2 when number of columns in in is less than ncol. ncol is counted from 1. */
int read_column (char *fname, int ncol, double **data)
{
    int i, n, ncols;
    double **datafile;

    /* buffer data file first in memory */
    n = read_datafile (fname, &datafile, &ncols);
    if (n < 0) return -1;

    if (ncol-1 < 0 || ncol-1 > ncols-1) return -2;

    *data = xmalloc (sizeof(double) * n);
    for (i=0; i<n; i++)
        (*data)[i] = datafile[i][ncol-1];

    return n;
}

int valid_dataline (char *s)
{
    return strspn (s, "0123456789eEdE.+- \r\n\t") == strlen (s);
}

typedef struct
{
    char    *fname;
    double  **data;
    int     ncols, nrec;
}
datafile_cache_t;

static datafile_cache_t *dc = NULL;
static int              ndc = 0, ndc_a = 0;

/* caches data file in memory for later column extraction */
int read_datafile (char *fname, double ***datafile, int *ncols)
{
    FILE *fp;
    int i, nrec_a, ignored, nc;
    char buf[16384], **L;

    /* see if file is already cached */
    if (dc != NULL)
    {
        for (i=0; i<ndc; i++)
        {
            if (strcmp (dc[i].fname, fname) == 0)
            {
                *datafile = dc[i].data;
                *ncols = dc[i].ncols;
                return dc[i].nrec;
            }
        }
    }

    /* expand file cache array if full or uninitialized */
    if (dc == NULL || ndc == ndc_a)
    {
        if (ndc_a == 0) ndc_a = 4;
        else            ndc_a *= 2;
        dc = xrealloc (dc, sizeof (datafile_cache_t) * ndc_a);
    }

    fp = fopen (fname, "r");
    if (fp == NULL) return -1;

    /* find out how many columns file has */
    while (fgets (buf, sizeof (buf), fp) != NULL)
    {
        if (valid_dataline(buf))
        {
            dc[ndc].ncols = str_words (buf, &L, WSEP_SPACES);
            free (L);
            break;
        }
    }
    /*warning1 ("file appeared to have %d columns\n", dc[ndc].ncols);*/

    /* now read file into array */
    rewind (fp);
    dc[ndc].nrec = 0;
    nrec_a = 1024;
    dc[ndc].data = xmalloc (sizeof (double *) * nrec_a);
    ignored = 0;
    while (fgets (buf, sizeof (buf), fp) != NULL)
    {
        if (!valid_dataline(buf))
        {
            ignored++;
            continue;
        }

        nc = str_words (buf, &L, WSEP_SPACES);
        if (nc != dc[ndc].ncols)
        {
            ignored++;
            continue;
        }

        if (dc[ndc].nrec == nrec_a)
        {
            nrec_a *= 2;
            dc[ndc].data = xrealloc (dc[ndc].data, sizeof (double *) * nrec_a);
        }

        dc[ndc].data[dc[ndc].nrec] = xmalloc (sizeof(double) * nc);
        for (i=0; i<nc; i++)
        {
            dc[ndc].data[dc[ndc].nrec][i] = atof (L[i]);
            /*warning1 ("assigned %f from '%s' to record %d, colunm %d\n",
                      dc[ndc].data[dc[ndc].nrec][i], L[i],
                      dc[ndc].nrec, i);*/
        }

        dc[ndc].nrec++;

        free (L);
    }

    fclose (fp);

    dc[ndc].fname = strdup (fname);
    ndc++;

    /*warning1 ("%d data records have been read; %d have been ignored\n",
              dc[ndc-1].nrec, ignored);*/
    *datafile = dc[ndc-1].data;
    *ncols = dc[ndc-1].ncols;

    /*for (i=0; i<dc[ndc-1].nrec; i++)
    {
        int j;
        printf ("rec %d: ", i);
        for (j=0; j<dc[ndc-1].ncols; j++)
            printf ("%g ", dc[ndc-1].data[i][j]);
        printf ("\n");
    }*/

    return dc[ndc-1].nrec;
}

/* */
void drop_datafile (char *fname)
{
    int i, j;

    if (strcmp (fname, "dropped data file") == 0) return;

    for (i=0; i<ndc; i++)
    {
        if (strcmp (dc[i].fname, fname) == 0)
        {
            free (dc[i].fname);
            dc[i].fname = "dropped data file";
            for (j=0; j<dc[i].nrec; j++) free (dc[i].data[j]);
            free (dc[i].data);
        }
    }
}

/* */
int interpolate_l (double *x1, double *y1, int n1, double *x2, double *y2, int n2)
{
    int  i, left, right, rc;

    if (n1 <= 0 || n2 <= 0) error1 ("n1=%d or n2=%d are invalid\n", n1, n2);

    for (i=0; i<n2; i++)
    {
        rc = bracket (x2+i, x1, n1, sizeof(double), cmp_doubles, &left, &right);
        switch (rc)
        {
        case 0:
            y2[i] = (y1[right] - y1[left]) / (x1[right] - x1[left]) *
               (x2[i] - x1[left]) + y1[left];
            break;

        case 1:
            y2[i] = y1[left];
            break;

        case -1:
            y2[i] = (y1[1] - y1[0]) / (x1[1] - x1[0]) *
               (x2[i] - x1[0]) + y1[0];
            break;

        case -2:
            y2[i] = (y1[n1-1] - y1[n1-2]) / (x1[n1-1] - x1[n1-2]) *
               (x2[i] - x1[n1-2]) + y1[n1-2];
            break;

        case -3:
            error1 ("x1 is empty\n");
        }
    }
    return 0;
}
