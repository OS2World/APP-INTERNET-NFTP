#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <asvtools.h>
#include "manycode.h"

#include <string.h>
#include <stdlib.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* returns number of translation pairs read from file, or -1 if error.
 allocates and sets 'in' and 'out'. */
int ucm_parse (char *filename, int **in, Wchar_t **out)
{
    FILE    *fp;
    char    buf[128], *p;
    Wchar_t w;
    int     i, mb_cur_min, mb_cur_max, c, preamble, N=0, Na;

    fp = fopen (filename, "r");
    if (fp == NULL) return -1;

    /* allocate space for conversion pair tables */
    Na = 256;
    *in  = malloc (sizeof(in[0]) * Na);
    *out = malloc (sizeof(out[0]) * Na);

    preamble = TRUE;
    while (fgets (buf, sizeof(buf), fp) != NULL)
    {
        /* skip comments */
        if (buf[0] == '#') continue;
        str_strip (buf, "\r\n");
        /* UCM preamble */
        if (buf[0] == '<' && preamble)
        {
            p = strchr (buf, '>');
            if (p == NULL) continue; /* malformed line */
            *p = '\0';
            if (strcmp (buf+1, "mb_cur_min") == 0)
            {
                mb_cur_min = atoi (p+1);
            }
            else if (strcmp (buf+1, "mb_cur_max") == 0)
            {
                mb_cur_max = atoi (p+1);
            }
            else if (strcmp (buf+1, "uconv_class") == 0)
            {
                str_strip2 (p+1, " \"");
                if (strcmp (p+1, "SBCS") == 0)
                {
                    /* single-byte charset: good! */
                }
                else
                {
                    error1 ("%s: not SBCS: not supported", filename);
                }
            }
        }
        /* actual conversion map */
        else if (buf[0] == '<' && !preamble)
        {
            /* ignore things not starting with <U */
            if (buf[1] != 'U') continue;
           /* <U0412>   \xF7      # kv020000 */
            /* cut off comment if present */
            p = strchr (buf, '#');
            if (p != NULL) *p = '\0';
            /* look up ending > in Unicode value */
            p = strchr (buf, '>');
            if (p == NULL) continue;
            *p = '\0';
            /* extract Unicode value */
            w = strtol (buf+2, NULL, 16);
           /* warning1 ("convert: [%s] -> %d\n", buf+2, w); */
            /* see where charset value is */
            p++;
            while (*p && (*p == ' ' || *p == '\t')) p++;
            if (*p == '\0') continue; /* malformed line; nothing after Unicode value */
            switch (*p)
            {
            case '\\':
                /* value starts with backslash */
                p++;
                if (*p == '\0') continue;
                c = 0;
                if (*p == 'x') c = strtol (p+1, NULL, 16);
                else error1 ("bogus charmap value [\\%s]", p);
                if (c < 0 || c > 256) error1 ("bogus charmap value %d", c);
                /* ignore chars <=0x7f; they are ASCII anyway */
               /* if (c <= 0x7f) continue; */
                if (c == 0) continue;
               /* warning1 ("found %d, %d\n", c, w); */
                /* see if this pair was already present. only last map is used */
                for (i=0; i<N; i++)
                {
                    if ((*in)[i] == c)
                    {
                        (*out)[i] = w;
                        break;
                    }
                }
                /* new one! add it */
                if (N == 0 || i == N)
                {
                    (*in)[N]  = c;
                    (*out)[N] = w;
                    N++;
                }
                /* allocate more space for pairs if needed */
                if (N == Na)
                {
                    Na *= 2;
                    *in  = realloc (*in,  sizeof(in[0]) * Na);
                    *out = realloc (*out, sizeof(out[0]) * Na);
                }
                break;
            default:
                /* bogus data */
                continue;
            }
        }
        /* watch for CHARMAP/END CHARMAP */
        else if (strcmp (buf, "CHARMAP") == 0)
        {
            preamble = FALSE;
        }
        else if (strcmp (buf, "END CHARMAP") == 0)
        {
            preamble = TRUE;
        }
    }
    
    fclose (fp);
    return N;
}
