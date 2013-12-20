#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char **filters;
static int  nfilters=0, *lengths;

/* return codes:
 -0 - success
 -1 - error loading
 -2 - invalid contents
  */
int load_ext_filters (char *fname)
{
    int i;

    filters = read_list (fname);
    if (filters == NULL) return -1;

    for (i=0; filters[i]!=NULL; i++)
    {
        if (strlen (filters[i]) < 2 ||
            (filters[i][0] != '-' && filters[i][0] != '+'))
            return -2;
    }
    if (i == 0) return -1;

    nfilters = i;
    lengths = xmalloc (sizeof(int) * nfilters);
    for (i=0; i<nfilters; i++)
        lengths[i] = strlen (filters[i]) - 1;

    return 0;
}

int unload_ext_filters (void)
{
    int i;

    if (nfilters <= 0) return -1;

    for (i=0; i<nfilters; i++)
        free (filters[i]);
    free (filters);
    free (lengths);

    return 0;
}

int check_ext_filters (char *s)
{
    int   i, len;
    char  *p;

    len = strlen (s);
    p = xmalloc (len+1);
    memcpy (p, s, len+1);
    strlwr (p);

    for (i=0; i<nfilters; i++)
    {
        if (lengths[i] <= len &&
            !strcmp (p+len-lengths[i], filters[i]+1))
        {
            free (p);
            return filters[i][0] == '+' ? TRUE : FALSE;
        }
    }

    /* include when not in the list */
    free (p);
    return TRUE;
}

#define MAX_LINE_LENGTH 65536

/* read file line-by-line and put every line into an array
 of malloc()ed strings. ignore empty strings and strings
 starting with '#'. return this array, NULL-terminated.
 returns NULL on error (file not found). every line in the
 returned array is malloc()ed. */
char **read_list (char *fname)
{
    char **list, *buffer;
    FILE *fp;
    int n, na;

    fp = fopen (fname, "r");
    if (fp == NULL) return NULL;

    buffer = xmalloc (MAX_LINE_LENGTH);
    n = 0;
    na = 16;
    list = xmalloc (sizeof(char *) * na);

    while (fgets (buffer, MAX_LINE_LENGTH, fp) != NULL)
    {
        str_strip (buffer, " \n\r");
        if (buffer[0] == '\0' || buffer[0] == '#') continue;

        /* 'na-1' since we will add one (NULL) record at the end! */
        if (n == na-1)
        {
            na *= 2;
            list = xrealloc (list, sizeof(char *) * na);
        }

        list[n++] = strdup (buffer);
    }

    fclose (fp);
    free (buffer);

    list = xrealloc (list, sizeof(char *) * (n+1));
    list[n] = NULL;

    return list;
}

static fastlist_t *fl1;
static char *str1;

static int cmp_simple (const void *e1, const void *e2)
{
    int *icr1, *icr2;

    icr1 = (int *)e1;
    icr2 = (int *)e2;

    return strcmp (fl1->rules[*icr1].rule, fl1->rules[*icr2].rule);
}

static int cmp_simple2 (const void *e1, const void *e2)
{
    int *icr1, *icr2;

    icr1 = (int *)e1;
    icr2 = (int *)e2;

    return strncmp (*icr1 == -1 ? str1 : fl1->rules[*icr1].rule,
                    *icr2 == -1 ? str1 : fl1->rules[*icr2].rule,
                    fl1->shortest);
}

static int kill_escapes (char *s)
{
    int escaped;
    char *p;

    escaped = FALSE;
    p = s;
    while (*p)
    {
        switch (*p)
        {
        case '\\':
            if (!escaped)
            {
                escaped = TRUE;
                p++;
            }
            else
                *s++ = *p++;
            break;
 
        default:
            if (escaped) escaped = FALSE;
            *s++ = *p++;
        }
    }
    *s = '\0';
    return 0;
}

/* prepare list for fast lookups */
fastlist_t *prepare_fastlist (char **list)
{
    int i, j, current_category, negation, escaped, first_special;
    fastlist_t *fl;
    char   *p, *p1;

    fl = xmalloc (sizeof(fastlist_t));
    /* count total number of rules. we add fictitios rule at the start
     to avoid having rule #0 which is bad for routines which use
     signed integers to indicate positive/negative matches */
    for (i=0; list[i] != NULL; i++)
        ;
    fl->n = i+1;
    fl->ncats = 0;
    fl->rules = xmalloc (sizeof(catrule_t) * fl->n);

    fl->rules[0].type     = RULETYPE_SEPARATOR;
    fl->rules[0].category = 0;
    fl->rules[0].rule = NULL;
    fl->rules[0].len = 0;

    /* process every rule: assign type, possibly convert */
    current_category = 0;
    for (i=1; i<fl->n; i++)
    {
        /* shifted because we inserted one rule! */
        p = list[i-1];

        /* check for separator */
        if (p[0] == '+')
        {
            p++;
            p1 = strchr (p, ' ');
            if (p1 != NULL) *p1 = '\0';
            current_category = atoi (p);
            if (current_category != 0) fl->ncats++;
            if (p1 != NULL) *p1 = ' ';
            fl->rules[i].type = RULETYPE_SEPARATOR;
            fl->rules[i].category = 0;
            fl->rules[i].rule = NULL;
            fl->rules[i].len = 0;
            continue;
        }

        /* check for subcategories list */
        if (p[0] == ':')
        {
            fl->rules[i].type = RULETYPE_SUBCATS;
            fl->rules[i].rule = strdup (p+1);
            fl->rules[i].category = current_category;
            continue;
        }

        /* check for negation */
        if (p[0] == '^')
        {
            p++;
            negation = TRUE;
        }
        else
            negation = FALSE;

        /* if rule does not contain slashes at all, append one at the
         end */
        if (strchr (p, '/') == NULL)
        {
            p = str_join (p, "/*");
        }

        /* see if this rule is simple, exact or complex: lookup
         special chars *, ?, [ */
        escaped = FALSE;
        first_special = -1;
        for (j=0; p[j] != '\0'; j++)
        {
            switch (p[j])
            {
            case '\\':
                if (!escaped) escaped = TRUE;
                break;

            case '?':
            case '*':
            case '[':
                if (!escaped) first_special = j;
                else          escaped = FALSE;
                break;

            default:
                if (escaped) escaped = FALSE;
            }
            if (first_special != -1) break;
        }

        /* no special chars at all? */
        if (first_special == -1)
        {
            if (negation)
                fl->rules[i].type = RULETYPE_MATCH_NEG;
            else
                fl->rules[i].type = RULETYPE_MATCH;
            fl->rules[i].rule = strdup (p);
            kill_escapes (fl->rules[i].rule);
        }
        /* simple rule? (just asterisk at the end) */
        else if (p[first_special] == '*' && p[first_special+1] == '\0')
        {
            if (negation)
                fl->rules[i].type = RULETYPE_SIMPLE_NEG;
            else
                fl->rules[i].type = RULETYPE_SIMPLE;
            fl->rules[i].rule = strdup (p);
            fl->rules[i].rule[first_special] = '\0';
            kill_escapes (fl->rules[i].rule);
        }
        /* everything else is complex */
        else
        {
            if (negation)
                fl->rules[i].type = RULETYPE_COMPLEX_NEG;
            else
                fl->rules[i].type = RULETYPE_COMPLEX;
            fl->rules[i].rule = strdup (p);
        }
        fl->rules[i].category = current_category;
        fl->rules[i].len = strlen (fl->rules[i].rule);
    }

    /* build a sorted list of simple and exact rules. find out
     the shortest rule among them */
    fl->n_simple = 0;
    fl->n_complex = 0;
    for (i=0; i<fl->n; i++)
    {
        if (fl->rules[i].type == RULETYPE_MATCH ||
            fl->rules[i].type == RULETYPE_SIMPLE ||
            fl->rules[i].type == RULETYPE_MATCH_NEG ||
            fl->rules[i].type == RULETYPE_SIMPLE_NEG)
            fl->n_simple++;
        else if (fl->rules[i].type == RULETYPE_COMPLEX ||
            fl->rules[i].type == RULETYPE_COMPLEX_NEG)
            fl->n_complex++;
    }

    if (fl->n_simple > 0)
        fl->simple = xmalloc (sizeof(int) * fl->n_simple);
    if (fl->n_complex > 0)
        fl->complex = xmalloc (sizeof(int) * fl->n_complex);
    fl->n_simple = 0;
    fl->n_complex = 0;
    fl->shortest = 10000000;
    /* j = 0; */
    for (i=0; i<fl->n; i++)
    {
        if (fl->rules[i].type == RULETYPE_MATCH ||
            fl->rules[i].type == RULETYPE_SIMPLE ||
            fl->rules[i].type == RULETYPE_MATCH_NEG ||
            fl->rules[i].type == RULETYPE_SIMPLE_NEG)
        {
            fl->shortest = min1 (fl->shortest, strlen(fl->rules[i].rule));
            fl->simple[fl->n_simple++] = i;
        }
        else if (fl->rules[i].type == RULETYPE_COMPLEX ||
                 fl->rules[i].type == RULETYPE_COMPLEX_NEG)
        {
            fl->complex[fl->n_complex++] = i;
        }
        /* if (fl->rules[i].type == RULETYPE_SEPARATOR) j = i+1; */
    }
    fl1 = fl;
    qsort (fl->simple, fl->n_simple, sizeof(int), cmp_simple);

    return fl;
}

/* an extended version of fnmatch1() which uses rules described
 in the gtsearch documentation */
static int fnmatch2 (char *patt, char *str, int notused)
{
    int  matched;
    char *p, *p1, *host, *path, *host_pattern, *path_pattern;

    p = NULL;
    /* see if patt contains any asterisks or if pattern begins
     with an asterisk */
    if (patt[0] == '*' || strchr (patt, '*') == NULL)
    {
        /* see if pattern contains slashes */
        p = strchr (patt, '/');
        if (p == NULL)
        {
            host_pattern = patt;
            path_pattern = NULL;
        }
        else
        {
            path_pattern = strdup (p);
            *p = '\0';
            host_pattern = patt;
        }
    }
    /* just an old-style pattern without any new extensions */
    else
    {
        return fnmatch1 (patt, str, 0);
    }

    /* split test string to hostname and path */
    p1 = strchr (str, '/');
    if (p1 == NULL)
    {
        host = str;
        path = NULL;
    }
    else
    {
        path = strdup (p1);
        *p1 = '\0';
        host = str;
    }

    /* now do the match */
    matched = TRUE;
    if (fnmatch1 (host_pattern, host, 0) != 0) matched = FALSE;
    if (/*matched &&*/ path != NULL && path_pattern != NULL &&
        fnmatch1 (path_pattern, path, 0) != 0) matched = FALSE;

    /* cleanup: restore original pattern if screwed,
     free allocated memory */
    if (p != NULL) *p = '/';
    if (p1 != NULL) *p1 = '/';
    if (path_pattern != NULL) free (path_pattern);
    if (path != NULL) free (path);

    return matched ? 0 : FNM1_NOMATCH;
}

#define MAXNUMHITS 8192

/* similar to check_list() but significantly faster on large rule lists */
int check_fastlist (fastlist_t *fl, char *str)
{
    int i, j, n, skip, icr, rc, left, right;
    int rulehit[MAXNUMHITS], n_included, n_excluded;

    /* if we are analyzing ruleset which mostly consists of complex rules
     do not try optimizations */
    if (fl->n_simple < fl->n_complex) goto straightforward_check;

    n_excluded = 0;
    n_included = 0;

    /* first we run our string through simple and complex rules */
    if (fl->n_simple > 0)
    {
        fl1 = fl;
        str1 = str;
        icr = -1;
        rc = bracket (&icr, fl->simple, fl->n_simple, sizeof(int),
                      cmp_simple2, &left, &right);
        switch (rc)
        {
        case -2: left = right = fl->n_simple-1; break;
        case -1: left = right = 0; break;
        case 0:
        case 1:  break;
        }
        for (i=left; i<=right; i++)
        {
            switch (fl->rules[fl->simple[i]].type)
            {
            case RULETYPE_MATCH:
                if (strcmp (fl->rules[fl->simple[i]].rule, str) == 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included++ + n_excluded] = fl->simple[i];
                }
                break;
    
            case RULETYPE_MATCH_NEG:
                if (strcmp (fl->rules[fl->simple[i]].rule, str) == 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included + n_excluded++] = - fl->simple[i];
                }
                break;
    
            case RULETYPE_SIMPLE:
                if (memcmp (fl->rules[fl->simple[i]].rule, str,
                            fl->rules[fl->simple[i]].len) == 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included++ + n_excluded] = fl->simple[i];
                }
                break;
    
            case RULETYPE_SIMPLE_NEG:
                if (memcmp (fl->rules[fl->simple[i]].rule, str,
                            fl->rules[fl->simple[i]].len) == 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included + n_excluded++] = - fl->simple[i];
                }
                break;
            }
        }
    }

    /* then we scan complex rules */
    for (i=0; i<fl->n_complex; i++)
    {
        switch (fl->rules[fl->complex[i]].type)
        {
        case RULETYPE_COMPLEX:
            if (fnmatch2 (fl->rules[fl->complex[i]].rule, str, 0) == 0)
            {
                if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                rulehit[n_included++ + n_excluded] = fl->complex[i];
            }
            break;

        case RULETYPE_COMPLEX_NEG:
            if (fnmatch2 (fl->rules[fl->complex[i]].rule, str, 0) == 0)
            {
                if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                rulehit[n_included + n_excluded++] = - fl->complex[i];
            }
            break;
        }
    }

    /* printf ("%d simple, %d complex, %d included, %d excluded\n", */
    /*        fl->n_simple, fl->n_complex, n_included, n_excluded); */

    /* two simple cases */
    if (n_included == 0) return FALSE;
    if (n_excluded == 0) return TRUE;

    /* more complex case. both included and excluded are present */
    n = n_included+n_excluded;
    qsort (rulehit, n, sizeof(int), cmp_absintegers);
    /* for (i=0; i<n; i++) printf ("%d ", rulehit[i]); */
    /* printf ("\n"); */

    /* we have the list of rules which fit to our sample. determining
     whether sample is included: we scan the list of rules. at first
     we have no category and sample is excluded. if current rule indicates
     'true' we signal 'included'. if current rule indicates 'false' we skip
     following rules which have the same category as current. at the end
     we signal 'excluded'. */
    skip = FALSE;
    for (i=0; i<n; /* don't increment!*/)
    {
        /* printf ("analyzing %d\n", rulehit[i]); */
        if (rulehit[i] > 0) return TRUE;
        if (rulehit[i] < 0)
        {
            for (j=i+1; j<n; j++)
            {
                if (fl->rules[abs(rulehit[j])].category !=
                    fl->rules[abs(rulehit[i])].category) break;
                /* printf ("skipping %d\n", rulehit[j]); */
            }
            i=j;
        }
        else
        {
            i++;
        }
    }
    return FALSE;

straightforward_check:

    /* we have two states while scanning the list of rules:
     1. we are looking at the patterns
     2. we are skipping to the next group terminator.
     Skipping occurs when we hit exclusion pattern. */
    skip = FALSE;
    for (i=0; i<fl->n; i++)
    {
        switch (fl->rules[i].type)
        {
        case RULETYPE_COMPLEX:
            if (skip) continue;
            if (fnmatch2 (fl->rules[i].rule, str, 0) == 0) return 1;
            break;

        case RULETYPE_COMPLEX_NEG:
            if (skip) continue;
            if (fnmatch2 (fl->rules[i].rule, str, 0) == 0) skip = TRUE;
            break;

        case RULETYPE_MATCH:
            if (skip) continue;
            if (strcmp (fl->rules[i].rule, str) == 0) return 1;
            break;

        case RULETYPE_MATCH_NEG:
            if (skip) continue;
            if (strcmp (fl->rules[i].rule, str) == 0) skip = TRUE;
            break;

        case RULETYPE_SIMPLE:
            if (skip) continue;
            if (memcmp (fl->rules[i].rule, str, fl->rules[i].len) == 0) return 1;
            break;

        case RULETYPE_SIMPLE_NEG:
            if (skip) continue;
            if (memcmp (fl->rules[i].rule, str, fl->rules[i].len) == 0) skip = TRUE;
            break;

        case RULETYPE_SEPARATOR:
            skip = FALSE;
            break;

        case RULETYPE_SUBCATS:
            /* ignored here */
            break;
        }
    }

    /* did not match any inclusion pattern; return 'ignore' */
    return 0;
}

/* returns list of categories assigned to given URL. uses fast
 representation */
int fast_category_check (fastlist_t *fl, char *str, int **categories)
{
    int  /*included, excluded,*/ rc, left, right;
    int  ncats, ncats_a, *cats;
    int  i, j, n, skip, icr;
    int  rulehit[MAXNUMHITS], n_included, n_excluded;
    /* double t1, t_simple, t_complex; */

    ncats_a = 8;
    ncats   = 0;
    cats    = xmalloc (sizeof(int) * ncats_a);

    /* if we are analyzing ruleset which mostly consists of complex rules
     do not try optimizations */
    if (fl->n_simple < fl->n_complex)
    {
        /* printf ("going to straightforward check; %d < %d\n", */
        /*        fl->n_simple, fl->n_complex); */
        goto straightforward_check;
    }

    n_excluded = 0;
    n_included = 0;

    /* first we run our string through simple and complex rules */
    /* t1 = clock1 (); */
    if (fl->n_simple > 0)
    {
        fl1 = fl;
        str1 = str;
        icr = -1;
        rc = bracket (&icr, fl->simple, fl->n_simple, sizeof(int),
                      cmp_simple2, &left, &right);
        switch (rc)
        {
        case -2: left = right = fl->n_simple-1; break;
        case -1: left = right = 0; break;
        case 0:
        case 1:  break;
        }
        for (i=left; i<=right; i++)
        {
            switch (fl->rules[fl->simple[i]].type)
            {
            case RULETYPE_MATCH:
                if (strcmp (fl->rules[fl->simple[i]].rule, str) == 0 &&
                    fl->rules[fl->simple[i]].category != 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included++ + n_excluded] = fl->simple[i];
                }
                break;
    
            case RULETYPE_MATCH_NEG:
                if (strcmp (fl->rules[fl->simple[i]].rule, str) == 0 &&
                    fl->rules[fl->simple[i]].category != 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included + n_excluded++] = - fl->simple[i];
                }
                break;
    
            case RULETYPE_SIMPLE:
                if (memcmp (fl->rules[fl->simple[i]].rule, str,
                            fl->rules[fl->simple[i]].len) == 0 &&
                    fl->rules[fl->simple[i]].category != 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included++ + n_excluded] = fl->simple[i];
                }
                break;
    
            case RULETYPE_SIMPLE_NEG:
                if (memcmp (fl->rules[fl->simple[i]].rule, str,
                            fl->rules[fl->simple[i]].len) == 0 &&
                    fl->rules[fl->simple[i]].category != 0)
                {
                    if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                    rulehit[n_included + n_excluded++] = - fl->simple[i];
                }
                break;
            }
        }
    }
    /* t_simple = clock1 () - t1; */

    /* then we scan complex rules */
    /* t1 = clock1 (); */
    for (i=0; i<fl->n_complex; i++)
    {
        switch (fl->rules[fl->complex[i]].type)
        {
        case RULETYPE_COMPLEX:
            if (fnmatch2 (fl->rules[fl->complex[i]].rule, str, 0) == 0 &&
                fl->rules[fl->complex[i]].category != 0)
            {
                if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                rulehit[n_included++ + n_excluded] = fl->complex[i];
            }
            break;

        case RULETYPE_COMPLEX_NEG:
            if (fnmatch2 (fl->rules[fl->complex[i]].rule, str, 0) == 0 &&
                fl->rules[fl->complex[i]].category != 0)
            {
                if (n_included+n_excluded == MAXNUMHITS) goto straightforward_check;
                rulehit[n_included + n_excluded++] = - fl->complex[i];
            }
            break;
        }
    }
    /* t_complex = clock1() - t1; */

    /* printf ("%d sim (%.3f sec), %d com (%.3f), %d inc, %d exc\n", */
    /*        fl->n_simple, t_simple, fl->n_complex, t_complex, n_included, n_excluded); */

    /* two simple cases */
    if (n_included == 0)
    {
        free (cats);
        return 0;
    }
    if (n_excluded == 0)
    {
        ncats = n_included + n_excluded;
        if (ncats > ncats_a)
        {
            ncats_a = ncats;
            free (cats);
            cats = xmalloc (sizeof(int) * ncats_a);
        }
        for (i=0, j=0; i<ncats; i++)
        {
            if (fl->rules[rulehit[i]].category != 0)
                cats[j++] = fl->rules[rulehit[i]].category;
        }
        ncats = j;
        if (ncats == 0) free (cats);
        else            *categories = cats;
        return ncats;
    }

    /* more complex case. both included and excluded are present */
    n = n_included+n_excluded;
    qsort (rulehit, n, sizeof(int), cmp_absintegers);
    /* for (i=0; i<n; i++) printf ("%d ", rulehit[i]); */
    /* printf ("\n"); */

    /* we have the list of rules which fit to our sample. determining
     to what categories sample belongs: we scan the list of rules. if
     current rule indicates 'true' we add its category to the list.
     if current rule indicates 'false' we skip following rules which
     have the same category as current. */
    skip = FALSE;
    for (i=0; i<n; /* don't increment!*/)
    {
        /* printf ("analyzing %d\n", rulehit[i]); */
        if (rulehit[i] > 0)
        {
            if (ncats == ncats_a)
            {
                ncats_a *= 2;
                cats = xrealloc (cats, sizeof(int)* ncats_a);
            }
            cats[ncats++] = fl->rules[rulehit[i]].category;
        }
        if (rulehit[i] < 0)
        {
            for (j=i+1; j<n; j++)
            {
                if (fl->rules[abs(rulehit[j])].category !=
                    fl->rules[abs(rulehit[i])].category) break;
                /* printf ("skipping %d\n", rulehit[j]); */
            }
            i=j;
        }
        else
        {
            i++;
        }
    }
    *categories = cats;
    return ncats;

straightforward_check:

    /* we have two states while scanning the list of rules:
     1. we are looking at the patterns
     2. we are skipping to the next group terminator.
     Skipping occurs when we hit exclusion pattern. */
    skip = FALSE;
    ncats = 0;
    for (i=0; i<fl->n; i++)
    {
        switch (fl->rules[i].type)
        {
        case RULETYPE_COMPLEX:
            if (skip) continue;
            if (fnmatch2 (fl->rules[i].rule, str, 0) == 0 &&
                fl->rules[i].category != 0)
            {
                if (ncats == ncats_a)
                {
                    ncats_a *= 2;
                    cats = xrealloc (cats, sizeof(int)* ncats_a);
                }
                cats[ncats++] = fl->rules[i].category;
            }
            break;

        case RULETYPE_COMPLEX_NEG:
            if (skip) continue;
            if (fnmatch2 (fl->rules[i].rule, str, 0) == 0) skip = TRUE;
            break;

        case RULETYPE_MATCH:
            if (skip) continue;
            if (strcmp (fl->rules[i].rule, str) == 0 &&
                fl->rules[i].category != 0)
            {
                if (ncats == ncats_a)
                {
                    ncats_a *= 2;
                    cats = xrealloc (cats, sizeof(int)* ncats_a);
                }
                cats[ncats++] = fl->rules[i].category;
            }
            break;

        case RULETYPE_MATCH_NEG:
            if (skip) continue;
            if (strcmp (fl->rules[i].rule, str) == 0) skip = TRUE;
            break;

        case RULETYPE_SIMPLE:
            if (skip) continue;
            if (memcmp (fl->rules[i].rule, str, fl->rules[i].len) == 0 &&
                fl->rules[i].category != 0)
            {
                if (ncats == ncats_a)
                {
                    ncats_a *= 2;
                    cats = xrealloc (cats, sizeof(int)* ncats_a);
                }
                cats[ncats++] = fl->rules[i].category;
            }
            break;

        case RULETYPE_SIMPLE_NEG:
            if (skip) continue;
            if (memcmp (fl->rules[i].rule, str, fl->rules[i].len) == 0) skip = TRUE;
            break;

        case RULETYPE_SEPARATOR:
            skip = FALSE;
            break;

        case RULETYPE_SUBCATS:
            break;
        }
    }

    if (ncats == 0) free (cats);
    else            *categories = cats;

    return ncats;
}

/*******************************************************
 check 'str' against NULL-terminated exclusion list 'excl'.
 if first symbol of exclusion pattern is ^, treat as
 exclude pattern, otherwise treat as include pattern.
 returns '1' if item is to be processed, and '0' if ignored.
 result depends on the order of pattern entries! this version
 supports pattern groups (lines beginning with '+' are treated
 as separators) */
int check_list (char **excl, char *str)
{
    int  i, skip;

    /* we have two states while scanning the list of rules:
     1. we are looking at the patterns
     2. we are skipping to the next group terminator.
     Skipping occurs when we hit exclusion pattern. */
    skip = FALSE;
    for (i=0; excl[i]; i++)
    {
        switch (excl[i][0])
        {
        case '^':
            /* exclusion pattern */
            if (skip) continue;
            if (!fnmatch2 (excl[i]+1, str, 0)) skip = TRUE;
            break;

        case '+':
            /* group terminator */
            skip = FALSE;
            break;

        default:
            /* inclusion pattern */
            if (skip) continue;
            if (!fnmatch2 (excl[i], str, 0)) return 1;
        }
    }

    /* did not match any inclusion pattern; return 'ignore' */
    return 0;
}

/*******************************************************
 check 'str' against exclusion list 'excl' which has 'n'
 elements (see check_list). pattern groups are NOT supported.
 returns '1' if 'str' is included, and '0' if not. */
int check_nlist (char **excl, int n, char *str)
{
    int  i;

    for (i=0; i<n; i++)
    {
        switch (excl[i][0])
        {
        case '^':
            /* exclusion pattern */
            if (!fnmatch2 (excl[i]+1, str, 0)) return 0;
            break;

        default:
            /* inclusion pattern */
            if (!fnmatch2 (excl[i], str, 0)) return 1;
        }
    }

    /* did not match any inclusion pattern; return 'ignore' */
    return 0;
}

