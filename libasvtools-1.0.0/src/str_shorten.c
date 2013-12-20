#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* -----------------------------------------------------------------------
 `str_shorten' shortens file name to FAT convention. This function does
 not check for existence of file with the same name; it's just an
 algorithm to reduce number of dots and symbols.
 parameter: s - original filename
 returns: statically allocated buffer with converted string, or NULL if failed
 -----------------------------------------------------------------------  */
char *str_shorten (char *s)
{
    char          *buffer = malloc (strlen(s)+1);
    char          name[8+1], extension[3+1], compression[3+1], *p;
    static char   fat_name[8+1+3+1];

    /* make a copy of original to screw it different ways */
    strcpy (buffer, s);

    /* kill symbols unwanted in FAT filenames */
    p = buffer;
    while (*p)
    {
        if (!str_fatsafe (*p))            str_delete (buffer, p);
        else                              p++;
    }
    
    /* remove dot at the end if present */
    str_strip (buffer, ".");
    if (buffer[0] == '\0') {free (buffer); return NULL;}

    /* replace starting dot with underscore if present */
    if (buffer[0] == '.') buffer[0] = '_';

    /* handle special case: .Z and .gz compressed files */
    compression[0] = '\0';
    if (str_tailcmpi (buffer, ".Z") == 0)  strcpy (compression, "z");
    if (str_tailcmp (buffer, ".gz") == 0)  strcpy (compression, "gz");
    if (compression[0] != '\0')
        *(strrchr (buffer, '.')) = '\0';

    /* get rid of unwanted dots in filename (replace them with underscore) */
    while (str_numchars (buffer, '.') > 1)
    {
        p = strchr (buffer, '.');
        *p = '_';
    }
    
    /* convert extension */
    p = strrchr (buffer, '.'); /* now p points to extension */
    if (p != NULL)
        str_reduce (extension, p+1, 3); /* sanify extension */
    else
        extension[0] = '\0';

    /* convert name */
    p = strchr (buffer, '.'); /* now p points to end of name */
    if (p != NULL) *p = '\0'; /* cut the rest */
    str_reduce (name, buffer, 8); /* sanify name */

    /* no longer need it */
    free (buffer);

    /* remember about compress/gzip? */
    if (compression[0] != '\0')
    {
        if (extension[0] == '\0')
            strcpy (extension, compression);
        else
            strcpy (extension+3-strlen(compression), compression);
    }

    /* combine name and extension */
    strcpy (fat_name, name);
    if (extension[0] != '\0')
    {
        strcat (fat_name, ".");
        strcat (fat_name, extension);
    }

    return fat_name;
}

/* returns 1 if 'c' is vowel, and 0 if not */
int str_isvowel (char c)
{
    static char *vowels =
        "AEIJOUY"
        "aeijouy";

    return strchr (vowels, c) != NULL ? 1 : 0;
}

/*
   And last... one suggestion: would it be possible to add an option to
   RECEIVE LONG FILENAMES TO A FAT drive?. The way I see this would be to
   STRIP VOCALS from the long filename, from left to right, until it fits
   8.3, and if it doesn't, then strip all vocals and chunk the filename
   at 8 chars (but keep the extension). (this is the mental scheme I use
   when renaming files by hand).
   
   I.e. "requirements.txt" becomes "reqrmnts.txt", "summertime.txt"
   becomes "summertm.txt" and where there is one file already with the
   same name, start adding digits from 1 to Z in the last (8th) char
   (i.e. summert1.txt, summert2.txt etc etc etc.).

                         --- Fernando Cassia <fcassia@theoffice.net>  */

/* makes string 's' one byte shorter */
void str_try_shorten (char *s)
{
    char *p;

    if (s[0] == '\0') return; /* sanity check */
    
    /* first try to delete special symbols */
    p = str_lastspn (s, "()[]{}~!@#$%^&");
    if (p != NULL) goto found;

    /* then look for dashes and underscores but leave leading ones in place */
    p = str_lastspn (s, "-_");
    if (p != NULL && p != s) goto found;

    /* then search for vowels */
    p = str_lastspn (s, "AEIJOUYaeijouy");
    if (p != NULL && p != s) goto found;

    /* then ? */
    p = str_lastspn (s,
                     "BCDFGHKLMNPQRSTVWXZ"
                     "bcdfghklmnpqrstvwxz");
    if (p != NULL && p != s) goto found;
    
    /* numbers are the last hope */
    p = str_lastspn (s, "0123456789");
    if (p != NULL) goto found;

    /* otherwise just cut the last byte */
    p = s + strlen (s) - 1;
    
found:
    str_delete (s, p);
}

/* returns 1 if 'c' can be used in FAT filename, or 0 if not */
int str_fatsafe (char c)
{
    static char *fat_allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "()[]{}~!@#$%^&.-_";

    return strchr (fat_allowed, c) != NULL ? 1 : 0;
}
