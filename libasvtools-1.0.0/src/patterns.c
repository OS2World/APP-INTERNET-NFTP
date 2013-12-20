#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int load_pattern_set (char *set_name, pattern_set *ps)
{
    int   i, mnp;
    char  *p, *p1;
    char  buffer[8192];

    ps->np = 0;

    if (infGetInteger (set_name, "max-number-of-patterns", &mnp) != 0) return -1;

    ps->patterns = xmalloc (sizeof (ps->patterns[0]) * mnp);

    for (i=0; i<mnp; i++)
    {
        snprintf1 (buffer, sizeof(buffer), "pattern-%d", i+1);
        if (infGetString (set_name, buffer, &p) == 0)
        {
            p1 = strchr (p, ',');
            if (p1 != NULL)
            {
                *p1 = '\0';
                ps->patterns[ps->np].type = p[0];
                ps->patterns[ps->np].patt = strdup (p1 + 1);

                if (ps->patterns[ps->np].type == 'x' &&
                    regcomp1 (&ps->patterns[ps->np].rx,
                              ps->patterns[ps->np].patt,
                              REG1_ICASE | REG1_NOSUB))
                    continue;

                if (ps->patterns[ps->np].type == 'X' &&
                    regcomp1 (&ps->patterns[ps->np].rx,
                              ps->patterns[ps->np].patt,
                              REG1_NOSUB))
                    continue;
            }
            ps->np++;
            free (p);
        }
    }

    ps->patterns = xrealloc (ps->patterns, sizeof (ps->patterns[0]) * ps->np);

    /* printf ("found %d patterns for %s:\n", ps->np, set_name); */
    /* for (i=0; i<ps->np; i++) */
    /*    printf ("%d. %c, %s\n", i, ps->patterns[i].type, ps->patterns[i].patt); */
    /* printf ("\n"); */

    return 0;
}

/* -------------------------------------------------------------------- */

int check_pattern (char *name, pattern_set *ps)
{
    int i, hit;

    hit = FALSE;
    for (i=0; i<ps->np && !hit; i++)
    {
        switch (ps->patterns[i].type)
        {
        case 's':
            if (str_stristr (name, ps->patterns[i].patt) != NULL)
                hit = TRUE;
            break;
        case 'S':
            if (strstr (name, ps->patterns[i].patt) != NULL)
                hit = TRUE;
            break;
        case 'x':
        case 'X':
            if (regexec1 (&ps->patterns[i].rx, name, 0, NULL, 0) == 0)
                hit = TRUE;
            break;
        case 'w':
            if (fnmatch1 (ps->patterns[i].patt, name, FNM1_CASEFOLD) == 0)
                hit = TRUE;
            break;
        case 'W':
            if (fnmatch1 (ps->patterns[i].patt, name, 0) == 0)
                hit = TRUE;
            break;
        case 'e':
            if (strcmp (name, ps->patterns[i].patt) == 0)
                hit = TRUE;
            break;
        case 'E':
            if (stricmp1 (name, ps->patterns[i].patt) == 0)
                hit = TRUE;
            break;
        }
    }

    return hit;
}
