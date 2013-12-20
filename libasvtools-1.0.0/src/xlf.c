#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#define isXdigit(c) ( (c>='0' && c<='9') || (c>='A' && c<='F') )

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* chars which are unsafe */
static char *unsafe = "#%,<>[]";

int xlf_need_escape (char c)
{
    unsigned char uc = c;
    if (uc < 32) return 1;
    if (strchr (unsafe, c) != NULL) return 1;
    return 0;
}

/* returns malloc()ed buffer with escaped version of 's' */
char *xlf_escape (char *s)
{
    unsigned char *us = (unsigned char *)s, *p;
    char buf[8];
    int n = 0, ip, op;

    /* find how many chars need to be escaped */
    while (*us)
    {
        n += xlf_need_escape (*us++);
    }

    /* nothing to escape? */
    if (n == 0) return strdup (s);

    p = xmalloc (strlen (s) + n*2 + 1);
    ip = 0, op = 0;
    while (s[ip])
    {
        if (xlf_need_escape (s[ip]))
        {
            snprintf1 (buf, sizeof(buf), "%%%02X", (unsigned char)s[ip]);
            memcpy (p+op, buf, 3);
            op += 3;
        }
        else
        {
            p[op++] = s[ip];
        }
        ip++;
    }
    p[op] = '\0';

    return (char *)p;
}

/* returns malloc()ed buffer with unescaped version of 's' */
char *xlf_unescape (char *s)
{
    char  *p;
    int   ip, op;

    p = xmalloc (strlen (s)+1);

    /* go through the string and seek %XX sequences */
    ip = 0, op = 0;
    while (s[ip])
    {
        if (s[ip] == '%' &&
            s[ip+1] && isXdigit(s[ip+1]) &&
            s[ip+2] && isXdigit(s[ip+2])
           )
        {
            p[op++] = hex2dec(s[ip+1])*16 + hex2dec(s[ip+2]);
            ip += 3;
        }
        else
        {
            p[op++] = s[ip++];
        }
    }
    p[op] = '\0';

    return p;
}
