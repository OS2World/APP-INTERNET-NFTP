#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <asvtools.h>
#include "manycode.h"
#include "ucs_tables.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define is_continue(c) (((c) & 0xC0) == 0x80)

/* converts one cp character to Unicode */
Wchar_t cp2w (unsigned char c, int codepage)
{
    unsigned char c1;
    int i;
    
    check_cp_table (codepage);

    for (i=0; ; i++)
    {
        c1 = CVT[codepage][i].c;
        if (c1 == 0) break;
        if (c1 == c) return CVT[codepage][i].w;
    }
    return (Wchar_t)'?';
}

/* converts one Unicode char to cp */
unsigned char w2cp (Wchar_t w, int codepage)
{
    int i;

    check_cp_table (codepage);

    for (i=0; ; i++)
    {
        if (CVT[codepage][i].c == 0) break;
        if (CVT[codepage][i].w == w)
        {
            return CVT[codepage][i].c;
        }
    }
    return '?';
}

/* gets 8-bit string in 'cp' and returns malloc()ed string in UTF-8 */
char *cp2utf (char *s, int cp)
{
    unsigned char  *s1, buf[7], *p;
    Wchar_t        w;
    int            n, N;
    
    if (s == NULL) return NULL;
    if (s[0] == '\0') return strdup ("");

    check_cp_table (cp);

    /* allocate large array */
    p = malloc (strlen(s)*6 + 1);
    
    s1 = (unsigned char *)s;
    N = 0;
    while (*s1)
    {
        w = cp2w (*s1, cp);
        n = wctoutf8 ((char *)buf, w);
        if (n == -1)
        {
            free (p);
            return NULL;
        }
        memcpy (p+N, buf, n);
        N += n; s1++;
    }
    p[N] = '\0';
    
    return (char *)p;
}

/* gets UTF-8 string and returns malloc()ed string in 'cp'. All non-cp
 chars are replaced by '?' */
char *utf2cp (char *s, int cp)
{
    unsigned char  *p, c;
    Wchar_t        w;
    int            n, N, r, state, l;
    
    if (s == NULL) return NULL;
    if (s[0] == '\0') return strdup ("");

    check_cp_table (cp);

    /* count how many bytes in the result we want */
    l = strlen (s);
    p = malloc (l+1);
    
    N = 0;
    r = 0;
    state = 0; /* 0 means no sequence, 1 means we're in sequence */
    while (s[N])
    {
        n = utf8towc (&w, s+N, l-N);
        if (n == -1)
        {
            if (state)
            {
                if (is_continue(s[N]))
                {
                    /* continuation char in sequence: ignore */
                }
                else
                {
                    /* sequence start while in sequence: place substitution */
                    p[r++] = '?';
                    state = 1;
                }
            }
            else
            {
                if (is_continue(s[N]))
                {
                    /* continuation char out of sequence: place substitution */
                    p[r++] = '?';
                }
                else
                {
                    /* sequence start while not in sequence: place substitution */
                    p[r++] = '?';
                    state = 1;
                }
            }
            N += 1;
        }
        else
        {
            state = 0;
            c = w2cp (w, cp);
            p[r++] = c;
            N += n;
        }
    }
    p[r] = '\0';
    
    return (char *)p;
}

/* tries to assign/load conversion table corresponding to 'cp'. returns:
 0 if found successfully, -1 if failed (in this case ISO8859-1 table
 is used instead */
int check_cp_table (int cp)
{
    int      i, n;
    char     ucmname[32], *p, *p1, *p2;
    int      *t_in;
    Wchar_t  *t_out;
    conv_table *cvt;

    /* first we check whether global meta-table is formed */
    if (CVT == NULL)
    {
        CVT = xmalloc (sizeof(CVT[0]) * (MAX_CODEPAGE+1));
        for (i=0; i<=MAX_CODEPAGE; i++)
        {
            CVT[i] = NULL;
        }
    }

    /* check validity of specified codepage */
    if (cp > MAX_CODEPAGE) error1 ("invalid codepage: %d", cp);

    /* see if this conversion table is already loaded */
    if (CVT[cp] != NULL) return 0;

    /* try to find built-in table */
    for (i=0; ; i++)
    {
        if (CVT0[i].cp == 0) break;
        if (CVT0[i].cp == cp)
        {
            CVT[cp] = CVT0[i].tbl;
            return 0;
        }
    }

    /* not a built-in table, try to load it from UCM file */
    snprintf1 (ucmname, sizeof (ucmname), "ibm-%d.ucm", cp);
    /* try ~/.ucm */
    p = getenv ("HOME");
    if (p != NULL)
    {
        p2 = str_sjoin (p, ".ucm");
        p1 = str_sjoin (p2, ucmname);
        free (p2);
        if (access (p1, R_OK) == 0) goto found;
    }
    /* try $UCM_PATH if exists, or /usr/local/share/ucm if not */
    p = getenv ("UCM_PATH");
    if (p != NULL)
    {
        p1 = str_sjoin (p, ucmname);
    }
    else
    {
        p1 = str_sjoin ("/usr/local/share/ucm", ucmname);
    }
    if (access (p1, R_OK) == 0) goto found;
    free (p1);
    /* try /usr/share/ucm */
    p1 = str_sjoin ("/usr/share/ucm", ucmname);
    if (access (p1, R_OK) == 0) goto found;
    free (p1);

    /* failed. assign ISO8859-1 and return -1 */
use_iso8859_1:
    warning1 ("warning: failed to find the table for %d\n", cp);
    CVT[cp] = CVT0[0].tbl;
    return -1;

found:
   /* warning1 ("found %s\n", p1); */
    n = ucm_parse (p1, &t_in, &t_out);
   /* warning1 ("parsing finished; n = %d\n", n); */
    free (p1);
    if (n <= 0) goto use_iso8859_1;
    cvt = malloc (sizeof(conv_table) * (n+1));
    for (i=0; i<n; i++)
    {
        cvt[i].c = t_in[i];
        cvt[i].w = t_out[i];
    }
    cvt[n].c = 0;
    cvt[n].w = 0;
    free (t_in); free (t_out);
    CVT[cp] = cvt;
    
    return 0;
}
