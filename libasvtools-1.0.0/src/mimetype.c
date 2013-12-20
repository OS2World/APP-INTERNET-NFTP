#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int mime_HTML, mime_PLAIN;

static struct mimetype_t
{
    char *type;
    int  next;
    char **extensions;
}
*mimetypes = NULL;

static int n = 0;

static int cmp_mimetypes (const void *e1, const void *e2)
{
    struct mimetype_t *m1, *m2;

    m1 = (struct mimetype_t *)e1;
    m2 = (struct mimetype_t *)e2;

    return strcmp (m1->type, m2->type);
}

/* loads MIME database from given file. a good example
 of such file is 'mime.types' supplied with Apache. */
int mime_load (char *filename)
{
    char **input, *p;
    int  i;

    input = read_list (filename);
    if (input == NULL || input[0] == NULL) return -1;

    /* count lines and allocate memory */
    for (n=0; ; n++)
    {
        if (input[n] == NULL) break;
    }
    if (n == 0) return -1;
    mimetypes = xmalloc (sizeof(mimetypes[0]) * n);

    /* assign and parse each line */
    for (i=0; i<n; i++)
    {
        mimetypes[i].type       = input[i];
        mimetypes[i].next       = 0;
        mimetypes[i].extensions = NULL;

        str_translate (mimetypes[i].type, '\t', ' ');
        p = strchr (mimetypes[i].type, ' ');
        if (p == NULL) continue;

        *p = '\0';
        mimetypes[i].next = str_words (p+1, &mimetypes[i].extensions, WSEP_SPACES);
        if (mimetypes[i].next <= 0)
        {
            mimetypes[i].next       = 0;
            mimetypes[i].extensions = NULL;
        }

#if 0
        /* this code exists simply for testing memory leaks. it is not
         * needed */
        {
            char **p = xmalloc (sizeof (char **));
            memcpy (p, mimetypes[i].extensions, sizeof (char **));
            free (mimetypes[i].extensions);
            mimetypes[i].extensions = p;
        }
#endif
    }

    /* sort assignments for quick location of names */
    qsort (mimetypes, n, sizeof(mimetypes[0]), cmp_mimetypes);

    free (input);

    mime_HTML  = mime_name2num ("text/html");
    mime_PLAIN = mime_name2num ("text/plain");
    if (mime_HTML < 0 || mime_PLAIN < 0) return -2;

    return 0;
}

/* unload mimetype database, free memory */
int mime_unload (void)
{
    int i;

    if (n > 0)
    {
        for (i=0; i<n; i++)
        {
            if (mimetypes[i].next > 0)
            {
                free (mimetypes[i].type);
                free (mimetypes[i].extensions);
            }
        }
        free (mimetypes);

        mimetypes = NULL;
        n = 0;
    }

    return 0;
}

/* return an integer corresponding to 'type'. returns -2 if
 MIME database was not initialized, -1 if 'type' is not found. */
int mime_name2num (char *type)
{
    struct mimetype_t *mt, mt1;

    if (mimetypes == NULL) return -2;

    mt1.type = type;
    mt = bsearch (&mt1, mimetypes, n,
                  sizeof(struct mimetype_t), cmp_mimetypes);
    if (mt == NULL) return -1;

    return (mt - mimetypes);
}

/* returns a pointer to the MIME type string corresponding to
 'num'. NULL is returned when MIME database was not initialized
 or 'num' is outside of range. */
char *mime_num2name (int num)
{
    if (mimetypes == NULL) return NULL;
    if (num < 0 || num >= n) return NULL;

    return mimetypes[num].type;
}

/* returns number of MIME types in the initialized database.
 negative values indicate that database was not initialized. */
int mime_ntypes (void)
{
    if (mimetypes == NULL) return -1;
    return n;
}
