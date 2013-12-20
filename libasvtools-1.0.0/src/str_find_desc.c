#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define skip_whitespace(p)  while (*p && (*p == ' ' || *p == '\t')) p++

/* --------------------------------------------------------------------------------------- */

static int   N=-1;
static char  **index1=NULL, **index2=NULL, *a1=NULL;

#define NL    0x0A
#define CR    0x0D

/* prepares to search for descriptions. 's' is the buffer where
 * descriptions will be looked up; unchanged upon exit */
void str_parseindex (char *s)
{
    char  *p1, *p2;
    int   n_a, l;

    /* free arrays from previous invocation if necessary */
    str_freeindex ();

    if (s == NULL) return;
    
    a1 = strdup (s);
    str_lower (a1);
    n_a = 32;
    index1 = xmalloc (sizeof(char *)*n_a);
    index2 = xmalloc (sizeof(char *)*n_a);

    N = 0;
    p1 = a1;
    while (*p1 && (*p1 == NL || *p1 == CR)) p1++;
    if (*p1 == '\0')
    {
        str_freeindex ();
        return; /* empty file */
    }
    p2 = p1;
    while (1)
    {
        while (*p2 && *p2 != NL && *p2 != CR) p2++;
        if (*p2 == '\0') break;
        *p2 = '\0';
        index1[N] = p1;
        /* cut a piece from s */
        l = p2-p1;
        index2[N] = xmalloc (l+1);
        memcpy (index2[N], s+(p1-a1), l);
        index2[N][l] = '\0';
        N++;
        if (N == n_a-2)
        {
            n_a *= 2;
            index1 = xrealloc (index1, sizeof(char *)*n_a);
            index2 = xrealloc (index2, sizeof(char *)*n_a);
        }
        p1 = p2 + 1;
        while (*p1 && (*p1 == NL || *p1 == CR)) p1++;
        if (*p1 == '\0') break; /* the end */
        p2 = p1;
    }
    
    index1 = xrealloc (index1, sizeof(char *)*N);
    index2 = xrealloc (index2, sizeof(char *)*N);
}

/* returns malloc()ed buffer with description which is looked in
 NULL-terminated buffer `s'. name of the file is `filename', size is `size'.
 when no description is found, NULL is returned */
char *str_finddesc (char *filename, unsigned long size)
{
    char            *p1, *s1;
    char            *d1, buf[16], *tmp_buf;
    unsigned char   *d;
    int             l, i;

    if (N == -1) return NULL; /* not initialized */

    s1 = strdup (filename);
    str_lower (s1);
    /* try to find appropriate line */
    p1 = NULL;
    for (i=0; i<N; i++)
    {
        if (str_headcmp (index1[i], s1) == 0)
        {
            /* 2. it should be followed by space, tab or '/' */
            p1 = index1[i] + strlen(s1);
            if (*p1 != ' ' && *p1 != '\t' && *p1 != '/' && *p1 != '\\') continue;
            /* 3. if it is followed by '/', then should be space or tab */
            if (*p1 == '/')
            {
                if (*(p1+1) != ' ' && *(p1+1) != '\t') continue;
                p1++;
            }
            /* assume we've found it! `p1' points to the start of description */
            p1++;
            break;
        }
    }
    free (s1);
    if (i == N) return NULL; /* not found */
    
    /* make a copy for screwing */
    l = strlen (p1);
    tmp_buf = malloc (l+1);
    memcpy (tmp_buf, index2[i]+(p1-index1[i]), l);
    tmp_buf[l] = '\0';
    d = (unsigned char *)tmp_buf;

    /* delete garbage and whitespace at the front of a line */
    skip_whitespace (d);
    if ((d[0] == '-' || d[0] == '=') && d[1] == ' ')
    {
        d += 2;
        skip_whitespace (d);
    }
    
    /* try to remove file size */
    if (size > 0)
    {
        snprintf1 (buf, sizeof(buf), "%lu ", size);
        d1 = strstr ((char *)d, buf);
        if (d1 != NULL) d = (unsigned char *)(d1 + strlen (buf));
        skip_whitespace (d);
    }

    /* skip that stupid "DIR" in hobbes descs */
    if (strncmp ((char *)d, "DIR ", 4) == 0) d += 4;
    skip_whitespace (d);
    
    /* try to remove timestamp */
    if (!memcmp (d, "Jan ", 4) || !memcmp (d, "Feb ", 4) ||
        !memcmp (d, "Mar ", 4) || !memcmp (d, "Apr ", 4) ||
        !memcmp (d, "May ", 4) || !memcmp (d, "Jun ", 4) ||
        !memcmp (d, "Jul ", 4) || !memcmp (d, "Aug ", 4) ||
        !memcmp (d, "Sep ", 4) || !memcmp (d, "Oct ", 4) ||
        !memcmp (d, "Nov ", 4) || !memcmp (d, "Dec ", 4))
    {
        d +=4;
        if (*d == '\0') return NULL;
        if ((isdigit (d[0]) || d[0] == ' ') && isdigit(d[1]) && d[2] == ' ')
        {
            d += 3;
            skip_whitespace (d);
            /* check for year 19xx or 20xx */
            if (((d[0] == '1' && d[1] == '9') ||
                 (d[0] == '2' && d[1] == '0')) &&
                isdigit (d[2]) && isdigit (d[3])) d += 4;
            /* check for time 00:00 */
            if (isdigit (d[0]) && isdigit (d[1]) && isdigit (d[3]) &&
                isdigit (d[4]) && d[2] == ':') d += 5;
        }
    }
    /* datetime in different format: 961021 */
    else if (isdigit (d[0]) && isdigit (d[1]) && isdigit (d[2]) &&
             isdigit (d[3]) && isdigit (d[4]) && isdigit (d[5]) &&
             d[6] == ' ')
    {
        d += 7;
    }
    /* datetime in 1998/01/01 format */
    else if (isdigit (d[0]) && isdigit (d[1]) && isdigit (d[2]) &&
             isdigit (d[3]) && d[4] == '/' && isdigit (d[5]) &&
             isdigit (d[6]) && d[7] == '/' && isdigit (d[8]) &&
             isdigit (d[9]) && d[10] == ' ')
    {
        d += 11;
    }
    skip_whitespace (d);

    str_strip ((char *)d, " \n\r\t");

    if (*d != '\0')
    {
        d = (unsigned char *)strdup ((char *)d);
        free (tmp_buf);
        return (char *)d;
    }
    else
    {
        free (tmp_buf);
        return NULL;
    }
}

/* frees internal structures used for descriptions lookup */
void str_freeindex (void)
{
    int i;

    if (N == -1) return;

    for (i=0; i<N; i++)
        free (index2[i]);
    free (index1);
    free (index2);

    if (a1 != NULL) free (a1);

    index1 = NULL;
    index2 = NULL;
    a1 = NULL;
    N = -1;
}

/* returns malloc()ed buffer with description which is looked in
 NULL-terminated buffer `s'. name of the file is `filename', size is `size'.
 when no description is found, NULL is returned */
char *str_find_description (char *s, char *filename, unsigned long size)
{
    char            *p, *p0, *p1;
    char            *d1, buf[16], *tmp_buf;
    unsigned char   *d;
    int             l;

    /* try to find appropriate line */
    p0 = s;
    while (1)
    {
        p = str_stristr (p0, filename);
        if (p == NULL) return NULL;
        p0 = p + strlen (filename);
        /* check for correct delimiters */
        /* 1. it should be either at the very beginning or preceded by CR or LF */
        p1 = p - 1;
        if (p != s && *p1 != '\n' && *p1 != '\r') continue;
        /* 2. it should be followed by space, tab or '/' */
        p1 = p + strlen(filename);
        if (*p1 != ' ' && *p1 != '\t' && *p1 != '/' && *p1 != '\\') continue;
        /* 3. if it is followed by '/', then should be space or tab */
        if (*p1 == '/')
        {
            if (*(p1+1) != ' ' && *(p1+1) != '\t') continue;
            p1++;
        }
        /* assume we've found it! `p1' points to the start of description */
        p1++;
        break;
    }

    /* make a copy for screwing */
    l = strcspn (p1, "\n\r");
    tmp_buf = malloc (l+1);
    strncpy (tmp_buf, p1, l);
    tmp_buf[l] = '\0';
    d = (unsigned char *)tmp_buf;

    /* delete garbage and whitespace at the front of a line */
    skip_whitespace (d);
    if ((d[0] == '-' || d[0] == '=') && d[1] == ' ')
    {
        d += 2;
        skip_whitespace (d);
    }
    
    /* try to remove file size */
    snprintf1 (buf, sizeof(buf), "%lu ", size);
    d1 = strstr ((char *)d, buf);
    if (d1 != NULL) d = (unsigned char *)(d1 + strlen (buf));
    skip_whitespace (d);

    /* skip that stupid "DIR" in hobbes descs */
    if (strncmp ((char *)d, "DIR ", 4) == 0) d += 4;
    skip_whitespace (d);
    
    /* try to remove timestamp */
    if (!memcmp (d, "Jan ", 4) || !memcmp (d, "Feb ", 4) ||
        !memcmp (d, "Mar ", 4) || !memcmp (d, "Apr ", 4) ||
        !memcmp (d, "May ", 4) || !memcmp (d, "Jun ", 4) ||
        !memcmp (d, "Jul ", 4) || !memcmp (d, "Aug ", 4) ||
        !memcmp (d, "Sep ", 4) || !memcmp (d, "Oct ", 4) ||
        !memcmp (d, "Nov ", 4) || !memcmp (d, "Dec ", 4))
    {
        d +=4;
        if (*d == '\0') return NULL;
        if ((isdigit (d[0]) || d[0] == ' ') && isdigit(d[1]) && d[2] == ' ')
        {
            d += 3;
            skip_whitespace (d);
            /* check for year 19xx or 20xx */
            if (((d[0] == '1' && d[1] == '9') ||
                 (d[0] == '2' && d[1] == '0')) &&
                isdigit (d[2]) && isdigit (d[3])) d += 4;
            /* check for time 00:00 */
            if (isdigit (d[0]) && isdigit (d[1]) && isdigit (d[3]) &&
                isdigit (d[4]) && d[2] == ':') d += 5;
        }
    }
    /* datetime in different format: 961021 */
    else if (isdigit (d[0]) && isdigit (d[1]) && isdigit (d[2]) &&
             isdigit (d[3]) && isdigit (d[4]) && isdigit (d[5]) &&
             d[6] == ' ')
    {
        d += 7;
    }
    /* datetime in 1998/01/01 format */
    else if (isdigit (d[0]) && isdigit (d[1]) && isdigit (d[2]) &&
             isdigit (d[3]) && d[4] == '/' && isdigit (d[5]) &&
             isdigit (d[6]) && d[7] == '/' && isdigit (d[8]) &&
             isdigit (d[9]) && d[10] == ' ')
    {
        d += 11;
    }
    skip_whitespace (d);

    str_strip ((char *)d, " \n\r\t");

    if (*d != '\0')
    {
        d = (unsigned char *)strdup ((char *)d);
        free (tmp_buf);
        return (char *)d;
    }
    else
    {
        free (tmp_buf);
        return NULL;
    }
}

/* writes file description into specified file.
 returns 0 on success and -1 if error occurs (cannot create description
 file, or cannot open it, or error while writing */
int write_description (char *filename, char *description, char *descfile)
{
    char            **desc = NULL;
    int             numdesclines = -1, newdesclen, rc, i;
    FILE            *fp;

    newdesclen = strlen (filename);
    
    /* check description file for existence and load it */
    if (access (descfile, F_OK) == 0 &&
        (numdesclines = load_textfile (descfile, &desc)) > 0)
    {
        /* remove old file */
        remove (descfile);
    }
    
    /* open new file again */
    fp = fopen (descfile, "w");
    if (fp != NULL)
    {
        /* write new description file */
        for (i=0; i<numdesclines; i++)
        {
            if (strncmp (desc[i], filename, newdesclen) == 0 &&
                (desc[i][newdesclen] == ' ' ||
                 desc[i][newdesclen] == '\t')) continue;
            if (desc[i][0] == '\0') continue;
            fprintf (fp, "%s\n", desc[i]);
        }
        fprintf (fp, "%s %s\n", filename, description);
        fclose (fp);
        rc = 0;
    }
    else
        rc = -1;

    if (numdesclines > 0)
    {
        free (desc[0]);
        free (desc);
    }
    return rc;
}
