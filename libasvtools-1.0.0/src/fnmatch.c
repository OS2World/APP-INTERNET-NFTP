#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#define	EOS	'\0'

#define RANGE_MATCH     1
#define RANGE_NOMATCH   0
#define RANGE_ERROR     (-1)

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static int rangematch (const char *, char, int, char **);

int
fnmatch1 (const char *pattern, const char *string, int flags)
{
    const char *stringstart;
    char *newp;
    char c, test;

    for (stringstart = string;;)
        switch (c = *pattern++) {
        case EOS:
            if ((flags & FNM1_LEADING_DIR) && *string == '/')
                return (0);
            return (*string == EOS ? 0 : FNM1_NOMATCH);
        case '?':
            if (*string == EOS)
                return (FNM1_NOMATCH);
            if (*string == '/' && (flags & FNM1_PATHNAME))
                return (FNM1_NOMATCH);
            if (*string == '.' && (flags & FNM1_PERIOD) &&
                (string == stringstart ||
                 ((flags & FNM1_PATHNAME) && *(string - 1) == '/')))
				return (FNM1_NOMATCH);
            ++string;
            break;
        case '*':
            c = *pattern;
            /* Collapse multiple stars. */
            while (c == '*')
                c = *++pattern;

            if (*string == '.' && (flags & FNM1_PERIOD) &&
                (string == stringstart ||
                 ((flags & FNM1_PATHNAME) && *(string - 1) == '/')))
                return (FNM1_NOMATCH);

            /* Optimize for pattern with * at end or before /. */
            if (c == EOS)
            {
                if (flags & FNM1_PATHNAME)
                {
                    return ((flags & FNM1_LEADING_DIR) ||
                            strchr(string, '/') == NULL ?
                            0 : FNM1_NOMATCH);
                }
                else
                    return (0);
            }
            else if (c == '/' && flags & FNM1_PATHNAME) {
                if ((string = strchr(string, '/')) == NULL)
                    return (FNM1_NOMATCH);
                break;
            }

            /* General case, use recursion. */
            while ((test = *string) != EOS) {
                if (!fnmatch1(pattern, string, flags & ~FNM1_PERIOD))
                    return (0);
                if (test == '/' && flags & FNM1_PATHNAME)
                    break;
                ++string;
            }
            return (FNM1_NOMATCH);
        case '[':
            if (*string == EOS)
                return (FNM1_NOMATCH);
            if (*string == '/' && (flags & FNM1_PATHNAME))
                return (FNM1_NOMATCH);
            if (*string == '.' && (flags & FNM1_PERIOD) &&
                (string == stringstart ||
                 ((flags & FNM1_PATHNAME) && *(string - 1) == '/')))
                return (FNM1_NOMATCH);

            switch (rangematch(pattern, *string, flags, &newp)) {
            case RANGE_ERROR:
                goto norm;
            case RANGE_MATCH:
                pattern = newp;
                break;
            case RANGE_NOMATCH:
                return (FNM1_NOMATCH);
            }
            ++string;
            break;
        case '\\':
            if (!(flags & FNM1_NOESCAPE)) {
                if ((c = *pattern++) == EOS) {
                    c = '\\';
                    --pattern;
                }
            }
            /* FALLTHROUGH */
        default:
        norm:
            if (c == *string)
                ;
            else if ((flags & FNM1_CASEFOLD) &&
                     (tolower((unsigned char)c) ==
                      tolower((unsigned char)*string)))
                ;
            else
                return (FNM1_NOMATCH);
            string++;
            break;
        }
    /* NOTREACHED */
}

static int
rangematch(const char *pattern, char test, int flags, char **newp)
{
    int negate, ok;
    char c, c2;

    /*
     * A bracket expression starting with an unquoted circumflex
     * character produces unspecified results (IEEE 1003.2-1992,
     * 3.13.2).  This implementation treats it like '!', for
     * consistency with the regular expression syntax.
     * J.T. Conklin (conklin@ngai.kaleida.com)
     */
    if ( (negate = (*pattern == '!' || *pattern == '^')) )
        ++pattern;

    if (flags & FNM1_CASEFOLD)
        test = tolower((unsigned char)test);

    /*
     * A right bracket shall lose its special meaning and represent
     * itself in a bracket expression if it occurs first in the list.
     * -- POSIX.2 2.8.3.2
     */
    ok = 0;
    c = *pattern++;
    do {
        if (c == '\\' && !(flags & FNM1_NOESCAPE))
            c = *pattern++;
        if (c == EOS)
            return (RANGE_ERROR);

        if (c == '/' && (flags & FNM1_PATHNAME))
            return (RANGE_NOMATCH);

        if (flags & FNM1_CASEFOLD)
            c = tolower((unsigned char)c);

        if (*pattern == '-'
            && (c2 = *(pattern+1)) != EOS && c2 != ']') {
            pattern += 2;
            if (c2 == '\\' && !(flags & FNM1_NOESCAPE))
                c2 = *pattern++;
            if (c2 == EOS)
                return (RANGE_ERROR);

            if (flags & FNM1_CASEFOLD)
                c2 = tolower((unsigned char)c2);

            if (c <= test && test <= c2)
                ok = 1;
        } else if (c == test)
            ok = 1;
    } while ((c = *pattern++) != ']');

    *newp = (char *)pattern;
    return (ok == negate ? RANGE_NOMATCH : RANGE_MATCH);
}
