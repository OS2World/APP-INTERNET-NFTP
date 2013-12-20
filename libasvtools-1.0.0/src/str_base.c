#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* returns 0 if string 's' starts with 'head', 1 if not */
int str_headcmp (char *s, char *head)
{
    int l = strlen (head);

    if (strlen (s) < l) return strcmp (s, head);
    return memcmp (s, head, l);
}

/* Due to unknown reasons, some systems have stricmp while others have
 strcasecmp. We don't want to depend on it */
int stricmp1 (char *s1, char *s2)
{
    char *s1a = s1, *s2a = s2;
    
    while (*s1a && *s2a)
    {
        if (tolower ((unsigned char)*s1a) != tolower ((unsigned char)*s2a)) break;
        s1a++; s2a++;
    }
    
    return (tolower ((unsigned char)*s1a) - tolower ((unsigned char)*s2a));
}

/* Due to unknown reasons, some systems have strnicmp while others have
 strncasecmp. We don't want to depend on it */
int strnicmp1 (char *s1, char *s2, int n)
{
    while (*s1 && --n > 0 &&
           tolower((unsigned char)*s1) == tolower((unsigned char)*s2))
	     s1++, s2++;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/* returns 0 if string 's' ends with 'tail', 1 if not */
int str_tailcmp (char *s, char *tail)
{
    int  ls = strlen (s), lt = strlen (tail);

    /* sanity checks */
    if (lt == 0) return 0;
    if (lt > ls) return 1;

    /* find the necessary position and do compare: */
    /* string:  abcdef  ls = 6 */
    /* tail:       def  lt = 3 */
    /* s+ls-lt:    def  ls-lt = 3 */
    return strcmp (s + ls - lt, tail);
}

/* returns 0 if string 's' ends with 'tail', 1 if not.
 compare is not case-sensitive */
int str_tailcmpi (char *s, char *tail)
{
    int  ls = strlen (s), lt = strlen (tail);

    /* sanity checks */
    if (lt == 0) return 1;
    if (lt > ls) return 0;

    /* find the necessary position and do compare: */
    /* string:  abcdef  ls = 6 */
    /* tail:       def  lt = 3 */
    /* s+ls-lt:    def  ls-lt = 3 */
    return stricmp1 (s + ls - lt, tail);
}

/* returns last character in 's' belonging to set in 'accept'; NULL if not found */
char *str_lastspn (char *s, char *accept)
{
    char *p;

    if (s[0] == '\0') return NULL; /* sanity check */
    
    p = s + strlen(s) - 1; /* 'p' now points to the last char of 's' */
    while (p >= s)
    {
        if (strchr (accept, *p) != NULL) return p;
        p--;
    }
    return NULL;
}

/* strstr() in case insensitive flavour */
char *str_stristr (char *haystack, char *needle)
{
    int   lh = strlen (haystack), ln = strlen (needle);
    char  *ph = malloc (lh+1), *pn = malloc (ln+1), *p;

    if (ph == NULL || pn == NULL) return NULL;

    strcpy (ph, haystack);
    strcpy (pn, needle);
    str_lower (ph);
    str_lower (pn);
    p = strstr (ph, pn);
    
    free (ph);
    free (pn);

    if (p == NULL) return NULL;
    return haystack + (p - ph);
}

/* returns position at which two strings are different. compare is
 not case sensitive */
int str_imatch (char *s1, char *s2)
{
    int i;
    for (i=0; s1[i] && s2[i]; i++)
        if (tolower((unsigned char)s1[i]) != tolower((unsigned char)s2[i]))
            break;
    return i;
}

/* index1 - finds unescaped character `needle' in the string `haystack'
 (ignore those protected by '\' and inside ") */

#define ESC_CHAR '\\'

char *str_index1 (char *haystack, char needle)
{
    char   *p;
    int    inside_quotes;
    
    if (haystack[0] == needle) return haystack;
    if (haystack[1] == '\0') return NULL;
    p = haystack+1;
    inside_quotes = 0;

    do
    {
        if (*p == needle && *(p-1) != ESC_CHAR && !inside_quotes)
            return p;
        if (*p == '"' && *(p-1) != ESC_CHAR)
            inside_quotes = 1 - inside_quotes;
        p++;
    }
    while (*p);
    
    return NULL;
}

/* does strncpy (dest, src, maxlength), then dest[maxlength-1] = 0. */
void str_copy (char *dest, char *src, int maxlength)
{
    strncpy (dest, src, maxlength);
    dest[maxlength-1] = '\0';
}

/* removes char at position 'p' from string 's'. \0 won't be removed */
void str_delete (char *s, char *p)
{
    if (p < s) return; /* simple sanity check */
    if (*p == '\0') return;
    memmove (p, p+1, strlen (p));
}

/* inserts char 'c' at position 'n' into string 's'. make sure 's' is long enough */
void str_insert (char *s, char c, int n)
{
    int l = strlen (s);
    
    if (n < 0) return;

    memmove (s+n+1, s+n, l-n+1);
    s[n] = c;
}

/* replaces sequences of multiple isspace() chars with single space.
 returns new length of the string */
int str_compress_spaces (char *s)
{
    int i, j, in_space;

    in_space = TRUE;
    for (i=0, j=0; s[i]!='\0'; i++)
    {
        if (isspace((unsigned char)s[i]))
        {
            if (in_space)
                continue;
            else
            {
                s[j++] = ' ';
                in_space = TRUE;
            }
        }
        else
        {
            s[j++] = s[i];
            in_space = FALSE;
        }
    }
    if (in_space) j--;
    s[j] = '\0';

    return j;
}

/* replaces all occurances of string s1 with with string s2 in the string s.
 returns number of changes made. recursive or not */
int str_replace (char *s, char *s1, char *s2, int recursive)
{
    char *p;
    int  n = 0, ln1, ln2;

    ln1 = strlen (s1);
    ln2 = strlen (s2);

    if (ln1 < ln2)
    {
        /* expansion! */
        p = s;
        while (1)
        {
            /* search for string to replace */
            p = strstr (p, s1);
            if (p == NULL) break;
            /* do replacing */
            if (ln1 != ln2)
                memmove (p+ln2, p+ln1, strlen(p+ln1)+1);
            memcpy (p, s2, ln2);
            n++;
            if (recursive) p = s;
            else           p += ln2;
        }
    }
    else
    {
        int i, j, ln, have_replacements;

        do
        {
            ln = strlen (s);
            have_replacements = FALSE;
            for (i=0, j=0; i<ln; )
            {
                if (ln-i >= ln1 && memcmp (s+i, s1, ln1) == 0)
                {
                    memcpy (s+j, s2, ln2);
                    i += ln1;
                    j += ln2;
                    have_replacements = TRUE;
                    n++;
                }
                else
                {
                    s[j++] = s[i++];
                }
            }
            s[j] = '\0';
        }
        while (recursive && have_replacements);
    }

    return n;
}

enum {SEEN_OTHER='o', SEEN_DOT='d', SEEN_SPACE='s'};

/* remove repeating sequences of '. .' */
void remove_dots (char *s)
{
    char *p, *q;
    int  state;

    if (s == NULL) return;

    /*warning1 ("remove_dots(%s)\n", s);*/
    /* replace '. .' sequences with '.', recursively */
    p = s;
    q = s;
    state = SEEN_OTHER;
    while (*p)
    {
        /*warning1 ("[%s] p-s1=%d, q-s1=%d, state=%c\n",
         s, p-s, q-s, state);*/
        switch (*p)
        {
        case '.':
            if (state == SEEN_SPACE)
            {
                /* rewind to previous dot and skip */
                q--;
                p++;
            }
            else
            {
                *q++ = *p++;
            }
            state = SEEN_DOT;
            break;

        case ' ':
            if (state == SEEN_DOT)
                state = SEEN_SPACE;
            else
                state = SEEN_OTHER;
            *q++ = *p++;
            break;

        default:
            state = SEEN_OTHER;
            *q++ = *p++;
            break;
        }
    }
    *q = '\0';
}

/* replaces repeated sequences of whitespace chars with single space */
void remove_whitespace (char *s)
{
    unsigned char *p, *q;

    q = (unsigned char *)s;
    p = (unsigned char *)s;

    /* first we skip whitespace at the beginning */
    while (*p && isspace (*p)) p++;

    for (; *p; /* no default action */)
    {
        if (isspace (*p))
        {
            *q++ = ' ';
            while (*p && isspace (*p)) p++;
        }
        else
        {
            *q++ = *p++;
        }
    }
    *q = '\0';

    /* then we strip whitespace at the end */
    if (q != (unsigned char *)s && isspace(*(q-1))) *(q-1) = '\0';
}

/* finds the token in `s' separated by `d', puts ending \0
 and returns pointer to token. advances `s' to point to first
 symbol of token. destructive */
char *str_sep1 (char **s, char d)
{
    char *p, *p1;

    /* skip delimiters before meaningful characters */
    p = *s;
    while (*p == d && *p) p++;
    if (*p == '\0') return NULL;

    /* find next delimiter. p now points to wanted token */
    p1 = strchr (p, d);
    if (p1 == NULL) return NULL;

    /* found. put delimiting 0x00 and return pointer to token */
    *p1 = '\0';
    *s = p1 + 1;
    return p;
}

/* str_copy is a safe version of strcpy. it does not copy past
 sizeof (dest), and after operation dest[sizeof(dest)-1] is always 0,
 thus ensuring that string is NULL-terminated */
void str_copy_worker (char *dest, char *src, int n)
{
    strncpy (dest, src, n);
    dest[n-1] = '\0';
}

/* copies `src' to `dest', reducing it to `dest_len' symbols */
void str_reduce (char *dest, char *src, int dest_len)
{
    char *b;
    int  l = strlen (src);

    if (dest_len <= 0) return; /* simple sanity check */
    if (l < dest_len)
    {
        /* no processing is required; string already fits */
        strncpy (dest, src, dest_len);
    }
    else
    {
        /* shorten it according Fernando's suggestion */
        b = malloc (l+1);
        strcpy (b, src);
        /* cut until it fits */
        while (strlen (b) > dest_len) str_try_shorten (b);
        strncpy (dest, b, dest_len);
        free (b);
    }
    dest[dest_len] = '\0'; /* make sure it is terminated */
}

/* concatenates two strings putting one and only one slash between them */
char *str_cats (char *dest, char *src)
{
    int l = strlen (dest);

    /* check: do we have slash in forward of `src'? */
    str_translate (src, '\\', '/');
    if (src[0] == '/')
    {
        if (l > 0 && dest[l-1] == '/')  dest[l-1] = '\0';
    }
    else
    {
        if (l > 0 && dest[l-1] != '/')  strcat (dest, "/");
    }
    strcat (dest, src);

    return dest;
}

/* copies string `s' into newly allocated buffer (just like strdup), but uses
 buffer N bytes longer than exactly needed for the string s. Returns NULL when
 malloc() fails */
char *str_strdup1 (char *s, int N)
{
    char *p;
    
    p = malloc (strlen(s) + 1 + N);
    if (p == NULL) return NULL;
    strcpy (p, s);
    return p;
}

/* removes chars from 'stray' at the tail of string 's' if present */
void str_strip (char *s, char *stray)
{
    char *s1;

    if (s[0] == '\0') return; /* nothing to do! */
    s1 = s + strlen (s) - 1; /* now s1 points to the last char in string s */
    while (s1 >= s && strchr (stray, *s1))
    {
        *s1-- = '\0'; /* kill char */
    }
}

/* removes chars from 'stray' at the head and tail of string 's' if present */
void str_strip2 (char *s, char *stray)
{
    char  *s1;

    if (s[0] == '\0') return; /* nothing to do! */

    /* skip all chars at the beginning */
    s1 = s;
    while (*s1 != '\0' && strchr(stray, *s1) != NULL) s1++;
    if (s1 != s) memmove (s, s1, strlen (s1)+1);

    /* empty? */
    if (s[0] == '\0') return;

    /* remove unwanted symbols at the end */
    s1 = s + strlen(s) - 1; /* now s1 points to the last char in string s */
    while (s1 >= s && strchr (stray, *s1))
    {
        *s1-- = '\0'; /* kill char */
    }
}

/* returns number of chars 'c' in string 's' */
int str_numchars (char *s, char c)
{
    int n = 0;
    
    while (*s)
        if (*s++ == c) n++;
    
    return n;
}

/* counts occurence of 'pattern' in 's' */
int str_numstr (char *s, char *pattern)
{
    char *p;
    int  n = 0;

    p = s;
    while (1)
    {
        p = strstr (p, pattern);
        if (p == NULL) break;
        n++;
        p += strlen (pattern);
    }
    return n;
}

/* replaces all occurances of char `in' with char `out' in string `s' */
void str_translate (char *s, char in, char out)
{
    while (*s)
    {
        if (*s == in) *s = out;
        s++;
    }
}

/* makes pathname look much, much better */
char *str_sanify (char *s)
{
    char  *p, *s1, *p1, *tail;

    s = str_strdup1 (s, 10);
    /* make sure we're not idiots */
    str_translate (s, '\\', '/');

    /* check for drive letter */
    if (strlen (s) > 1 && s[1] == ':')
        s1 = s + 2;
    else
        s1 = s;
    strcat (s1, "/");

    /* destroy "/./"s */
    while ((p = strstr (s1, "/./")) != NULL)
    {
        memmove (p, p+2, strlen(p+2)+1);
    }

    /* get rid of .. components */
    while ((p = strstr (s1, "/../")) != NULL)
    {
        tail = strdup (p+3);       /* save the tail */
        *p = '\0';                 /* cut at "/../" */
        p1 = strrchr (s1, '/');    /* look for previous / */
        if (p1 == NULL)            /* not found! */
            strcpy (s1, tail);     /* assume the tail is what we want */
        else
            strcpy (p1, tail);     /* put tail at previous / */
        free (tail);
    }

    /* make sure result does not end with unnecessary '/' */
    str_strip (s1+1, "/");

    /* drive letters are always case insensitive */
    if (strlen(s) > 1 && s[1] == ':') s[0] = tolower ((unsigned char)s[0]);

    return s;
}

/* joins two strings s1 and s2 and returns pointer to malloc()ed space
 which holds the result */
char *str_join (char *s1, char *s2)
{
    char *s;

    s = malloc (strlen (s1) + strlen (s2) + 1);
    strcpy (s, s1);
    strcat (s, s2);

    return s;
}

/* appends s2 to s1 in malloc()ed space, frees s1, returns pointer to new
 string */
char *str_append (char *s1, char *s2)
{
    int  l1, l2;
    char *s;

    if (s1 == NULL)
    {
        return strdup (s2);
    }
    else
    {
        l1 = strlen (s1);
        l2 = strlen (s2);
        s = malloc (l1 + l2 + 1);
        if (s == NULL) return NULL;
        memcpy (s, s1, l1);
        free (s1);
        memcpy (s+l1, s2, l2+1);
    }

    return s;
}

/* joins n strings from array L putting sep between them.
 returns malloc()ed buffer; separators aren't placed at the
 beginning and the end of the resulting string */
char *str_mjoin (char **L, int n, char sep)
{
    char *p;
    int  i, l, nc;

    l = n-1; /* separators */
    for (i=0; i<n; i++)
        l += strlen (L[i]);
    l += 1; /* terminating NULL */

    p = malloc (l);
    nc = 0;
    for (i=0; i<n; i++)
    {
        l = strlen (L[i]);
        memcpy (p+nc, L[i], l);
        nc += l;
        if (i != n-1)
            p[nc++] = sep;
    }
    p[nc] = '\0';

    return p;
}

/* joins n strings from array L putting 'sep' between them.
 returns malloc()ed buffer; separators aren't placed at the
 beginning and the end of the resulting string */
char *str_msjoin (char **L, int n, char *sep)
{
    char *p;
    int  i, l, nc, lsep;

    lsep = strlen (sep);
    l = (n-1)*lsep; /* separators */
    for (i=0; i<n; i++)
        l += strlen (L[i]);
    l += 1; /* terminating NULL */

    p = malloc (l);
    nc = 0;
    for (i=0; i<n; i++)
    {
        l = strlen (L[i]);
        memcpy (p+nc, L[i], l);
        nc += l;
        if (i != n-1)
        {
            strcpy (p+nc, sep);
            nc += lsep;
        }
    }
    p[nc] = '\0';

    return p;
}

/* returns malloc()ed concatenation of s1 and s2, and slash between */
char *str_sjoin (char *s1, char *s2)
{
    char *p = malloc (strlen(s1) + strlen(s2) + 2);
    strcpy (p, s1);
    str_cats (p, s2);
    return p;
}

#define NEWLINE    0x0A
#define CARRIAGE   0x0D

/* str_refine_buffer() breaks one long string into number of small ones
 at LF/CRs. returns the total length of resulting string */
int str_refine_buffer (char *buf)
{
    char *src = buf, *dest = buf;
    
    while(*src)
    {
        if (*src == NEWLINE || *src == CARRIAGE)
        {
            if ((*src == CARRIAGE && *(src+1) == NEWLINE) ||
                (*src == NEWLINE && *(src+1) == CARRIAGE)) src++;
            *dest = '\0';
        }
        else
            *dest = *src;
        src++;
        dest++;
    }
    *dest = '\0';
    return dest-buf+1;
}

/* decomposes 's' into 'path' and 'name' components. takes care
 about drive letters if necessary. returns malloc()ed path and name;
 if 's' does not contain path components, 'path' is NULL */
void str_pathdcmp (char *s, char **path, char **name)
{
    char *s1, *p;

    s1 = str_sanify (s);

    /* see if it has path components */
    p = strrchr (s1, '/');
    if (p == NULL)
    {
        if (s1[1] != ':')
        {
            *path = NULL;
            *name = strdup (s1);
        }
        else
        {
            *path = str_strdup1 (s1, 3);
            (*path)[2] = '\0';
            if (s1[2] == '\0') *name = NULL;
            else               *name = strdup (s1+2);
        }
    }
    else
    {
        *p = '\0';
        *path = str_strdup1 (s1, 1);
        /* append / if drive letter only */
        if (strlen(*path) == 2 && (*path)[1] == ':') strcat (*path, "/");
        if (*(p+1) == '\0') *name = NULL;
        else                *name = strdup (p+1);
    }
}

static char english_lower_table [256] =
{
 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
 0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

/* lowercases all chars in the string. english only! */
void str_lower (char *s)
{
    while (*s)
    {
        *s = english_lower_table[*((unsigned char *)s)];
        s++;
    }
}

/* lowercases char; english only! */
char tolower1 (char c)
{
    return english_lower_table[(unsigned char)c];
}

/* makes pathname look much, much better */
void sanify_pathname (char *s)
{
    char  *p, *s1, *p1, buf[8192];
    int   l;

    /* make sure we're not idiots */
    str_translate (s, '\\', '/');

    /* check for drive letter */
    if (strlen (s) > 1 && s[1] == ':')
        s1 = s + 2;
    else
        s1 = s;
    str_cats (s1, "");

    /* destroy "/./"s */
    while ((p = strstr (s1, "/./")) != NULL)
    {
        strcpy (p, p+2);
    }

    /* get rid of .. components */
    while ((p = strstr (s1, "/../")) != NULL)
    {
        strcpy (buf, p+3);       /* save the tail */
        *p = '\0';               /* cut at "/../" */
        p1 = strrchr (s1, '/');   /* look for previous / */
        if (p1 == NULL)          /* not found! */
        {
            strcpy (s1, "/");
            strcat (s1, buf);     /* assume the tail is what we want */
        }
        else
            strcpy (p1, buf);    /* put tail at previous / */
    }

    /* make sure result does not end with unnecessary '/' */
    l = strlen (s1);
    while (l > 1 && s1[l-1] == '/')
    {
        s1[l-1] = '\0';
        l = strlen (s1);
    }

    /* drive letters are always case insensitive */
    if (strlen(s) > 1 && s[1] == ':') s[0] = tolower ((unsigned char)s[0]);

    return;
}

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define quad_t   int64_t
#define u_quad_t u_int64_t

#ifndef LLONG_MAX
#define LLONG_MAX       0x7fffffffffffffffLL    /* max value for a long long */
#endif

#ifndef LLONG_MIN
#define LLONG_MIN       (-0x7fffffffffffffffLL - 1)  /* min for a long long */
#endif

/* Quads and long longs are the same size.  Ensure they stay in sync. */
#define QUAD_MAX        LLONG_MAX       /* max value for a quad_t */
#define QUAD_MIN        LLONG_MIN       /* min value for a quad_t */

/*
 * Convert a string to a quad integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
quad_t strtoq1(const char *nptr, char **endptr,	register int base)
{
	register const char *s;
	register u_quad_t acc;
	register unsigned char c;
	register u_quad_t qbase, cutoff;
	register int neg, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	s = nptr;
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for quads is
	 * [-9223372036854775808..9223372036854775807] and the input base
	 * is 10, cutoff will be set to 922337203685477580 and cutlim to
	 * either 7 (neg==0) or 8 (neg==1), meaning that if we have
	 * accumulated a value > 922337203685477580, or equal but the
	 * next digit is > 7 (or 8), the number is too big, and we will
	 * return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	qbase = (unsigned)base;
	cutoff = neg ? (u_quad_t)-(QUAD_MIN + QUAD_MAX) + QUAD_MAX : QUAD_MAX;
	cutlim = cutoff % qbase;
	cutoff /= qbase;
	for (acc = 0, any = 0;; c = *s++) {
		if (!isascii(c))
			break;
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= qbase;
			acc += c;
		}
	}
	if (any < 0) {
		acc = neg ? QUAD_MIN : QUAD_MAX;
		/* errno = ERANGE; */
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}

/* breaks given line 's' using 'sep' as separators. creates an array of
 string pointers in 'L' and returns number of them. each string in the
 result is malloc()ed */
int str_parseline (char *s, char ***L, char sep)
{
    int   na, n;
    char  *p, *p1, **L1, str[2];
    
    n  = 0;
    na = 16;
    str[0] = sep;
    str[1] = '\0';
    
    L1 = malloc (sizeof (char *) * na);
    p = s;
    do
    {
        if (n == na)
        {
            na *= 2;
            L1 = realloc (L1, sizeof (char *) * na);
        }
        p1 = strchr (p, sep);
        if (p1 != NULL) *p1 = '\0';
        L1[n++] = strdup (p);
        str_strip (L1[n-1], str);
        p = p1 + 1;
    }
    while (p1 != NULL);

    *L = L1;
    return n;
}

#if !defined(HAVE_STRLWR)
/* convert string to lower case using current locale */
char *strlwr (char *s)
{
    unsigned char *us = s;

    while (*us)
    {
        *us = tolower (*us);
        us++;
    }
    
    return s;
}
#endif

#if !defined(HAVE_STRUPR)
/* convert string to upper case using current locale */
char *strupr (char *s)
{
    unsigned char *us = s;

    while (*us)
    {
        *us = toupper (*us);
        us++;
    }
    
    return s;
}
#endif

/* case insensitive strstr */
char *str_casestr (char *str, char *substr)
{
#if HAVE_STRCASESTR
    return strcasestr (str, substr);
#else
    char *strn, *substrn;/* [strlen(str)+1], substrn[strlen(substr)+1]; */
    char *p;

    strn = malloc (strlen(str)+1);
    substrn = malloc (strlen(substr)+1);
    if (substr == NULL || substrn == NULL) return NULL;

    strcpy (strn,str);
    strcpy (substrn,substr);

    strlwr (strn);
    strlwr (substrn);

    p = strstr (strn,substrn);
    if (p != NULL) p = str+(p-strn);

    free (strn);
    free (substrn);

    return p;
#endif
}

/* cuts off string tail to max. 'limit' chars and pads with '...' */
void str_cutoff (char *s, int limit)
{
    int   ln;
    unsigned char  *p;

    ln = strlen (s);
    if (ln < limit) return;

    /* point to the place where we going to cut it */
    p = (unsigned char *)(s + limit - 3);
    while (p > (unsigned char *)s && isalnum (*p)) p--;
    if (p == (unsigned char *)s) p = (unsigned char *)(s + limit - 3);

    strcpy ((char *)p, "...");

}

static char *find_letter (char *s, int sep_class)
{
    switch (sep_class)
    {
    case WSEP_SPACES:
        while (*s && isspace((unsigned char)*s)) s++; break;

    case WSEP_WILDCARD:
        while (*s && (isspace((unsigned char)*s) || *s == '*' || *s == '?')) s++; break;
        
    case WSEP_COMMAS:
        while (*s && *s == ',') s++; break;
        
    case WSEP_SEMICOLON:
        while (*s && *s == ';') s++; break;
        
    case WSEP_PERIOD:
        while (*s && *s == '.') s++; break;
        
    case WSEP_VBAR:
        while (*s && *s == '|') s++; break;

    case WSEP_COLON:
        while (*s && *s == ':') s++; break;

    case WSEP_LETTERS:
        while (*s && !isalpha((unsigned char)*s)) s++; break;

    case WSEP_TEXT:
    default:
        while (*s && !isalnum((unsigned char)*s)) s++; break;
    }

    if (*s == '\0') return NULL;
    return s;
}

static char *find_nonletter (char *s, int sep_class)
{
    switch (sep_class)
    {
    case WSEP_SPACES:
        while (*s && !isspace((unsigned char)*s)) s++; break;

    case WSEP_WILDCARD:
        while (*s && !isspace((unsigned char)*s) && *s != '*' && *s != '?') s++; break;
        
    case WSEP_COMMAS:
        while (*s && *s != ',') s++; break;
        
    case WSEP_SEMICOLON:
        while (*s && *s != ';') s++; break;
        
    case WSEP_PERIOD:
        while (*s && *s != '.') s++; break;
        
    case WSEP_VBAR:
        while (*s && *s != '|') s++; break;
        
    case WSEP_COLON:
        while (*s && *s != ':') s++; break;
        
    case WSEP_LETTERS:
        while (*s && isalpha((unsigned char)*s)) s++; break;

    case WSEP_TEXT:
    default:
        while (*s && isalnum((unsigned char)*s)) s++; break;
    }

    if (*s == '\0') return NULL;
    return s;
}

/* breaks given line 's' into words. creates an array of
 string pointers in 'L' and returns number of them. each string in the
 result is a pointer to a place in 's'. original 's' is not preserved.
 sep_class determines what separators between words are assumed */
int str_words (char *s, char ***L, int sep_class)
{
    int   na, n;
    char  *p1, *p2, **L1;

    if (sep_class == WSEP_NONE)
    {
        *L = xmalloc (sizeof (char *));
        (*L)[0] = s;
        return 1;
    }
    
    n  = 0;
    na = 16;

    L1 = xmalloc (sizeof (char *) * na);
    p1 = s;
    do
    {
        if (n == na)
        {
            na *= 2;
            L1 = xrealloc (L1, sizeof (char *) * na);
        }
        p1 = find_letter (p1, sep_class);
        if (p1 == NULL) break;
        p2 = find_nonletter (p1, sep_class);
        if (p2 != NULL) *p2 = '\0';
        L1[n++] = p1;
        p1 = p2 + 1;
    }
    while (p2 != NULL);
    
    if (n == 0)
    {
        free (L1);
        *L = NULL;
    }
    else
    {
        L1 = xrealloc (L1, sizeof (char *) * n);
        *L = L1;
    }

    return n;
}

/* breaks given line 's' into words. creates an array of
 string pointers in 'L' and returns number of them. each string in the
 result is a pointer to a place in 's'. original 's' is not preserved.
 sep is a separator between words */
int str_split (char *s, char ***L, char sep)
{
    int   na, n;
    char  *p, *p1, **L1;
    
    n  = 0;
    na = 16;
    
    L1 = xmalloc (sizeof (char *) * na);
    p = s;
    do
    {
        if (n == na)
        {
            na *= 2;
            L1 = xrealloc (L1, sizeof (char *) * na);
        }
        p1 = strchr (p, sep);
        if (p1 != NULL) *p1 = '\0';
        L1[n++] = p;
        p = p1 + 1;
    }
    while (p1 != NULL);

    *L = L1;
    return n;
}
