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

/* initializes all fields of ftp URL to empty or reasonable values */
void init_url (url_t *u)
{
    u->hostname[0]    = '\0';
    u->userid[0]      = '\0';
    u->password[0]    = '\0';
    u->pathname[0]    = '\0';
    u->description[0] = '\0';
    u->ip             = 0L;
    u->port           = 21;
    u->flags          = 0;
}

/* parses given 'url' and gives values to 's' elements.
 returns 0 when no errors, 1 if failed (empty URL)
 ftp://name:pw@hostname:6000/directory
 ftp://asv:xxx@localhost:5000:d:/apps/emtex
 nondestructive on url */
int parse_url (char *url, url_t *s)
{
    char    *p1, *p2, *p, *url1, *url2;

   /* put initial values into the structure */
    init_url (s);

   /* skip "ftp://" part if necessary */
    if (strnicmp1 (url, "ftp://", 6) == 0) url += 6;

   /* make a copy */
    url1 = strdup (url);
    
   /* make sure all slashes are backward */
    str_translate (url1, '\\', '/');

   /* check and analyze login/passwd: "@" before "/" ? */
    p1 = strchr (url1, '/');
    p2 = strchr (url1, '@');
    if (p2 != NULL && (p1 == NULL || p2 < p1))
    {
        *p2 = '\0';
        p1 = strchr (url1, ':');
        if (p1 != NULL)
        {
            str_scopy (s->password, p1+1);
            *p1 = '\0';
        }
        strcpy (s->userid, url1);
        url2 = p2 + 1;
    }
    else
        url2 = url1;

   /* look for port/path */
    p1 = strchr (url2, ':');
    p2 = strchr (url2, '/');
    p  = p1 == NULL ? p2 : (p2 == NULL ? p1 : (p2 > p1 ? p1 : p2));

   /* p+1 now points to path/port; before it is hostname */
   /* somesite.net:6000/private */
   /*             ^p */

   /* look for port number */
    if (p != NULL)
    {
        if (*p == ':')
        {
            if (isdigit( (int) (*(p+1)) ))
            {
               /* port found */
                p1 = p + 1 + strspn (p+1, "0123456789");
               /* somesite.net:6000/private */
               /*             ^p   ^p1 */
                if (p1 != NULL)
                {
                   /* copy pathname (everything starting at p1) */
                    str_scopy (s->pathname, *p1 == ':' ? p1+1 : p1);
                    *p1 = '\0';
                }
                s->port = atoi (p+1);
            }
            else
            {
               /* copy pathname (everything starting at p+1) */
                str_scopy (s->pathname, p+1);
            }
        }
        else
        {
           /* copy pathname (everything starting at p) */
            str_scopy (s->pathname, p);
        }
        *p = '\0';
    }
    
   /* anything before : or / is hostname */
    str_scopy (s->hostname, url2);

   /* fix pathname about %20 thingies */
    dehexify (s->pathname);

   /* fix things like /c:/path !!! */
   /* if (s->pathname[0] == '/' && s->pathname[2] == ':') str_delete (s->pathname, s->pathname); */

    free (url1);
    
    return (s->hostname[0] == '\0');
}

/* composes URL from given structure. returns malloc()ed buffer.
 when pass=1, write explicit password into the file. makes more 'safe'
 URLs suitable for feeding into command-line programs, not only WWW browsers */
char *compose_url (url_t u, char *filename, int pass, int squidy)
{
    char buffer[8192], buf2[64], *p;
    int  is_anonymous;

    is_anonymous = (stricmp1 (u.userid, "anonymous") == 0 ||
                    stricmp1 (u.userid, "ftp") == 0);
    if (u.port != 21)
        snprintf1 (buf2, sizeof(buf2), ":%u", u.port);
    else
        buf2[0] = '\0';
    pass = pass && (u.password[0] != '\0');

    snprintf1 (buffer, sizeof(buffer), "%s%s%s",
               u.pathname,
               u.pathname[0] == '\0' ? "" : "/",
               filename);

    p = hexify (buffer);

    if (!squidy)
    {
        snprintf1 (buffer, sizeof(buffer), "ftp://%s%s%s%s%s%s%s%s",
                   is_anonymous ? "" : u.userid,
                   is_anonymous ? "" : (pass ? ":" : ""),
                   is_anonymous ? "" : (pass ? u.password : ""),
                   is_anonymous ? "" : "@",
                   u.hostname,
                   buf2,
                   u.pathname[0] == '/' ? "" : ":",
                   p);
    }
    else
    {
        snprintf1 (buffer, sizeof(buffer), "ftp://%s%s%s%s%s%s/%%2f%s",
                   is_anonymous ? "" : u.userid,
                   is_anonymous ? "" : (pass ? ":" : ""),
                   is_anonymous ? "" : (pass ? u.password : ""),
                   is_anonymous ? "" : "@",
                   u.hostname,
                   buf2,
                   p);
    }
    free (p);
    str_replace (buffer+6, "//", "/", 1);
    return strdup (buffer);
}

/* normalize URL: delete 'http://' if present, delete port
 number if default. replace backslashes with forward slashes. conversion
 is done in-place. returns 0 on success, and -1 on malformed URLs */
int normalize_url (char *url)
{
    char *p, *p1;

    /* delete http:// */
    if (str_headcmp (url, "http://") == 0)
    {
        memmove (url, url+7, strlen (url)-6);
    }

    str_translate (url, '\\', '/');
   /* str_strip (url, "/"); */
    p = strstr (url, ":80/");
    if (p != NULL)
    {
        p1 = strchr (url, '/');
        /* make sure we don't hit :80 inside the URI:
         our 'p' must be before all occurances of '/' */
        if (p1 > p)
        {
            memmove (p, p+3, strlen(p)-2);
        }
    }

    return 0;
}

/* normalize URL: delete 'http://' if present, delete port
 number if default. conversion is done in-place. */
int normalize_url2 (char *url) {
    char *p, *p1;

    /* delete http:// */
    if (str_headcmp (url, "http://") == 0) {
        memmove (url, url+7, strlen (url)-6);
    }

    /* str_strip (url, "/") */
    p = strstr (url, ":80/");
    if (p != NULL) {
        p1 = strchr (url, '/');
        /* make sure we don't hit :80 inside the URI:
         our 'p' must be before all occurances of '/' */
        if (p1 > p) {
            memmove (p, p+3, strlen(p)-2);
        }
    }

    return 0;
}


#define MAX_HOST_LENGTH   64
#define MAX_PATH_LENGTH 1024
#define MAX_STR_LENGTH  8192

/* --------------------------------------------------------------------
 returns 0 on success, 1 if URL is invalid, 2 if not HTTP URL,
 and -1 on errors. 'hostname'
 and 'pathname' are malloc()ed or NULL if not in 'url'.
 Generic URL can look like (spaces are for readability!):
 [protocol:[//]] [username[:password]@] [hostname] [:port] [pathname]
 */
int parse_http_url (char *url, char **hostname, int *port, char **pathname)
{
    char *p, *p1;
    char ref[MAX_STR_LENGTH], host[MAX_HOST_LENGTH], path[MAX_PATH_LENGTH];
    int  i, rc, portnum, http_detected, hostname_detected;

    /* -------- Defaults --------------------------------- */

    host[0] = '\0';
    portnum = 80;
    strcpy (path, "/");

    /* ------ Basic URL preprocessing -------------------- */

    /* ignore too long URLs */
    if (strlen (url) > MAX_STR_LENGTH) return 1;

    /* copy link to 'ref' discarding \r and \n */
    p = url;
    for (i=0; *p; p++)
    {
        if (*p != '\r' && *p != '\n')
            ref[i++] = *p;
    }
    ref[i] = '\0';

    /* strip leading/trailing spaces */
    str_strip2 (ref, " \t");

    /* ignore empty links and links with spaces inside */
    if (ref[0] == '\0' || strchr (ref, ' ') != NULL) return 1;

    /* ignore anchors within the same page */
    if (ref[0] == '#') return 1;

    /* strip anchor if present */
    if ((p = strchr (ref, '#')) != NULL) *p = '\0';

    /* replace '\' with '/'. stupid microsoft server accepts '\' as '/'! */
    str_translate (ref, '\\', '/');

    /* ------------ Protocol abbreviation --------------------- */

    /* see if link contains protocol abbreviation. the only accepted
     protocol is http */
    http_detected = FALSE;
    if ((p = strchr (ref, ':')) != NULL)
    {
        /* chars before ':' must be latin alpha */
        p1 = ref;
        while ((*p1 >= 'a' && *p1 <= 'z') || (*p1 >= 'A' && *p1 <= 'Z')) p1++;
        if (p1 == p)
        {
            if (p-ref == 4 && memcmp (ref, "http", 4) == 0)
            {
                /* discard 'http:' */
                memmove (ref, ref+5, strlen (ref+5)+1);
                /* discard two slashes if present */
                if (str_headcmp (ref, "//") == 0)
                    memmove (ref, ref+2, strlen (ref+2)+1);
                http_detected = TRUE;
            }
            else
                return 2;
        }
    }

    /* see if link starts with '//'. assume that 'http:' is omitted */
    if (!http_detected && str_headcmp (ref, "//") == 0)
    {
        memmove (ref, ref+2, strlen (ref+2)+1);
        http_detected = TRUE;
    }

    /* ------------- Detect username/password --------------------- */

    if ((p = strchr (ref, '@')) != NULL)
    {
        /* @ can be either in username construction or in the path
         (not in sitename), therefore if @ happened before first
         slash it indicates username */
        p1 = strchr (ref, '/');
        /* ignore URLs with username/password */
        if (p1 == NULL || p < p1) return 1;
    }

    /* ------------- Pathname --------------------------- */

    /* remaining part is  [sitename] [:port] [pathname]. however
     we have to distinguish between relative path (pub/archive) and
     hostname/path (pub.org/archive). if we have seen 'http://' before
     and starting fragment of path looks like hostname we assume that
     it IS hostname. if we have seen 'http://' before and starting path
     does not look like hostname and does not start with '/' we return error.
     this catches one common bug (http:///file.html) and rejects other
     malformings */
    hostname_detected = FALSE;
    if (http_detected)
    {
        p = strchr (ref, '/');
        if (p != NULL) *p = '\0';
        p1 = strchr (ref, ':');
        if (p1 != NULL) *p1 = '\0';
        if (valid_hostname (ref) && strlen (ref) < MAX_HOST_LENGTH)
        {
            hostname_detected = TRUE;
        }
        else
        {
            /* check if ref starts with '/'! */
            if (p != ref) return 1;
        }
        if (p1 != NULL) *p1 = ':';
        if (p != NULL) *p = '/';
    }

    /* separate path from hostname */
    if (hostname_detected)
    {
        /* not all remaining string is path */
        if ((p = strchr (ref, '/')) != NULL)
        {
            /* everything starting from slash is path */
            if (strlen (p) >= MAX_PATH_LENGTH) return 1;
            strcpy (path, p);
            *p = '\0';
        }
    }
    else
    {
        /* everything which remains is path */
        if (strlen (ref) >= MAX_PATH_LENGTH) return 1;
        strcpy (path, ref);
        /* nothing is left unparsed */
        ref[0] = '\0';
    }

    /* -------------- Sitename, port --------------------- */

    /* ref now contains  [sitename] [:port] */
    if ((p = strchr (ref, ':')) != NULL)
    {
        /* check that port is valid (i.e. not empty and does not
         contain garbage */
        p1 = p+1;
        if (*p1 == '\0' || strspn (p1, "0123456789") != strlen (p1)) return 1;
        portnum = atoi (p1);
        *p = '\0';
        /* be paranoid */
        if (portnum < 80) return 1;
    }

    /* check remaining part (if any) for being valid hostname */
    if (ref[0] != '\0')
    {
        if (!valid_hostname (ref)) return 1;
        if (strlen (ref) >= MAX_HOST_LENGTH) return 1;
        strcpy (host, ref);
        /* normalized hostname must be in lower case */
        strlwr (host);
    }

    /* ---------------- Refine pathname ------------------------------ */

    /* if hostname isn't present we don't need to refine pathname */
    if (host[0] != '\0')
    {
        rc = refine_pathname (path);
        if (rc) return rc;

        /* if hostname is not empty, path must be absolute (this is actually
         algorithm sanity check) */
        if (path[0] != '/') return 1;
    }

    /* ------------------ Set up results ----------------- */

    if (host[0] != '\0') *hostname = strdup (host);
    else                 *hostname = NULL;

    if (path[0] != '\0') *pathname = strdup (path);
    else                 *pathname = NULL;

    *port = portnum;

    return 0;
}

/* returns TRUE is 's' looks like valid FDQN hostname and FALSE if not */
int valid_hostname (char *s)
{
    int len;
    char *p;

    len = strlen (s);

    /* 1) server name consists of digits and dots only; we don't
     accept numeric addresses */
    if (strspn (s, "0123456789.") == len) return FALSE;

    /* 2) must start and end with alphanumeric */
    if (!isalnum((unsigned char)s[0]) || !isalpha ((unsigned char)s[len-1])) return FALSE;

    /* 3) must contain at least one dot */
    if (strchr (s, '.') == NULL) return FALSE;

    /* 4) must not contain multi-dot sequences */
    if (strstr (s, "..") != NULL) return FALSE;

    /* 5) server name must consist of a-z, A-Z, 0-9, '.-_' (note:
     underscore is not permitted by RFC952 but we still allow it) */
    p = s;
    while (*p)
    {
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '.' ||
            *p == '-' ||
            *p == '_')
        {
            p++;
        }
        else
            return FALSE;
    }

    return TRUE;
}

/* returns non-zero when path is invalid */
int refine_pathname (char *path)
{
    char *p, *p1, *tail;

    /* get rid of './path', 'path/.', 'path/..' things */
    if (str_headcmp (path, "./") == 0) memmove (path, path+2, strlen (path+2)+1);
    if (str_tailcmp (path, "/.") == 0) path[strlen(path)-1] = '\0';
    if (str_tailcmp (path, "/..") == 0)
    {
        path[strlen(path)-3] = '\0';
        p = strrchr (path, '/');
        if (p == NULL) return 1;
        *(p+1) = '\0';
    }

    /* get rid of '/./' components */
    str_replace (path, "/./", "/", TRUE);

    /* get rid of '..' components */
    while ((p = strstr (path, "/../")) != NULL)
    {
        tail = p+3;                /* save the tail */
        *p = '\0';                 /* cut at "/../" */
        p1 = strrchr (path, '/');  /* look for previous / */
        if (p1 == NULL)            
            return 1;              /* not found! */
        else
            memmove (p1, tail, strlen(tail)+1); /* put tail at previous / */
    }

    /* replace repeating slashes with single one */
    str_replace (path, "//", "/", TRUE);

    return 0;
}

/* returns 0 on success, -1 on error and 1 if link is invalid, 2 if not HTTP.
 algorithm:
 1. parse 'url' into 'hostname', 'portnum', 'pathname'
 2. if 'hostname' is valid (not NULL) 'url' is complete, i.e. contains
 all parts (host, port, path). we just pass along results of the parse_url
 3. 'hostname' is NULL: 'url' is partial. partial url cannot contain port
 number. it can only contain path which can be relative or absolute. if
 'pathname' is NULL, this is an error (cannot happen).
 4. if 'pathname' starts with '/' (absolute pathname) we use it.
 5. cut base_pathname '?' (remove script parameters), locate last slash
 and join 'pathname' after it.
 */
int join_links (char *base_hostname, int base_port,
                char *base_pathname, char *url,
                char **host, int *port, char **path)
{
    char *hostname, *pathname, *base, *p;
    int  rc, portnum;

    /* parse given link */
    rc = parse_http_url (url, &hostname, &portnum, &pathname);
    if (rc) return rc;

    /* if it was absolute just pass it along */
    if (hostname != NULL)
    {
        *host = hostname;
        *port = portnum;
        *path = pathname;
        return 0;
    }
    else
    {
        /* hostname is empty. use base_hostname */
        *host = strdup (base_hostname);
        *port = base_port;
    }

    if (pathname == NULL)
    {
        free (*host);
        return -1;
    }

    /* if pathname is absolute, use it. otherwise concatenate */
    if (pathname[0] == '/')
    {
        *path = pathname;
    }
    else
    {
        base = strdup (base_pathname);
        /* cut off script parameters */
        if ((p=strrchr (base, '?')) != NULL) *p = '\0';

        /* join */
        p = strrchr (base, '/');

        if (p == NULL)
        {
            /* can't happen ('base' always starts with slash) */
            free (base);
            free (*host);
            free (pathname);
            return -1;
        }

        /* located slash in 'base' */
        p++;
        *p='\0';
        *path = str_sjoin (base, pathname);
        free (base);
        free (pathname);

        rc = refine_pathname (*path);
        if (rc)
        {
            free (*host);
            free (*path);
            return rc;
        }
    }

    return 0;
}
