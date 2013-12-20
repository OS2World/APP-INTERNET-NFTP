#include "fsearch.h"

#include <asvtools.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static char *glob2regexp (char *glob, int case_sen);
static char *regexcase (char *rx);
static void prospero_unescape (char *s);
static void prospero_escape (char *s);

static char *unix_distrib_excludes =
    "!slackware:!Slackware:"
    "!redhat:!RedHat:!ftp.redhat.com:!SRPMS:"
    "!Mandrake:!mandrake:"
    "!SuSE:!suse:"
    "!blackcat:"
    "!KSI:!Nostromo:"
    "!slink:!potato:!debian:"
    "!OpenLinux:!Caldera:"
    "!TurboLinux:"
    "!OpenBSD:!openbsd:"
    "!NetBSD:!netbsd:"
    "!freebsd:!FreeBSD:!-RELEASE:!packages:!2.x-STABLE:!3.x-STABLE:!-current:"
    "!CPAN:!cpan";

static char *picture_ext[] =
{"jpg", "jpeg", "jpe", "gif", "pcx", "tif", "tiff", "bmp", "xpm", "xbm",
"pnm", "pbm", "png", "img", "pcd", "tga", "pic"};
#define npict  (sizeof(picture_ext)/sizeof(picture_ext[0]))

static char *sound_ext[] =
{"mp3", "wav", "au", "snd", "mid", "midi", "kar", "s3m", "stm", "xm", "aif", "aiff", "ra"};
#define nsound  (sizeof(sound_ext)/sizeof(sound_ext[0]))

static char *movie_ext[] =
{"avi", "mpg", "mpeg", "mpe", "mov", "qt"};
#define nmovie  (sizeof(movie_ext)/sizeof(movie_ext[0]))

#define NUM_RESULTS_SLOTS 1024

Results *RR;
int     *slots;

/* -----------------------------------------------------------------------------------------
 *
 */

static void init_slots (void)
{
    int i;

    RR    = xmalloc (sizeof(RR[0]) * NUM_RESULTS_SLOTS);
    slots = xmalloc (sizeof(int) * NUM_RESULTS_SLOTS);
    for (i=0; i<NUM_RESULTS_SLOTS; i++)
    {
        slots[i] = 0;
    }
}

static void free_slot (int ns)
{
    slots[ns] = 0;
}

static int find_slot (void)
{
    int i;

    for (i=0; i<NUM_RESULTS_SLOTS; i++)
    {
        if (slots[i] == 0)
        {
            slots[i] = 1;
            return i;
        }
    }
    return -1;
}

/* ====================================================================================== */

int FTPS_query (char *target, int maxhits, int mmtype,
                int req_type, int case_sen, int distrib_exclude,
                char *path_inc, char *path_exc,
                char *dom_inc, char *dom_exc,
                long start_date, long end_date,
                long min_size, long max_size)
{
    char       cmd[4096], buffer[64], buf1[4096], method;
    char       *newtarget, *p, *p1, *p2;
    struct tm  tm1, tm2;
    int        skip_first_colon, index, match;
    time_t     time1;

    if (req_type == ST_NAVIGATE)
    {
        snprintf1 (cmd, sizeof(cmd),
                 "VERSION 5 query-1.0\n"
                 "AUTHENTICATE '' UNAUTHENTICATED query-user\n"
                 "DIRECTORY ASCII ARCHIE'/HOST/%s'\n"
                 "LIST ATTRIBUTES COMPONENTS\n",
                 target);
        goto request_ready;
    }

    newtarget = strdup (target);

    // see if target has spaces in it
    if (strchr (newtarget, ' ') != NULL)
    {
        
    }
    
    method = '\0';
    // check whether multimedia type has been specified
    if (mmtype != 0)
    {
        match = FALSE;
        switch (req_type)
        {
        case ST_EXACT: /* error! */
        case ST_NAVIGATE:
            free (newtarget);
            return -1;

        case ST_M_REGEXP:
            match = TRUE;
            
        case ST_S_REGEXP: /* make sure search target isn't ending with $ */
            p = regexcase (newtarget);
            free (newtarget);
            newtarget = p;
            str_strip (newtarget, "$");
            break;
            
        case ST_M_SUBSTR:
            match = TRUE;
            
        case ST_S_SUBSTR: /* substring: build a new target and use regex */
            p = regexcase (newtarget);
            free (newtarget);
            newtarget = p;
            break;
            
        case ST_M_GLOB:
            match = TRUE;

        case ST_S_GLOB:
            p = glob2regexp (newtarget, FALSE);
            str_strip (p, "$");
            free (newtarget);
            newtarget = p;
            break;
        }

        /* here we build actual query string (regexp). the structure is: nftp\.(avi|mov|mpeg) */
        case_sen = FALSE;
        method = match ? 'X' : 'R';
        switch (mmtype)
        {
        case MT_PICTURE:  p = str_mjoin (picture_ext, npict, '|'); break;
        case MT_SOUND:    p = str_mjoin (sound_ext, nsound, '|'); break;
        case MT_MOVIE:    p = str_mjoin (movie_ext, nmovie, '|'); break;
        default:
            free (newtarget);
            return -1;
        }
        snprintf1 (buf1, sizeof(buf1), "%s.*\\.(%s)$", newtarget, p);
        free (p);
        p = strdup (buf1);
        free (newtarget);
        newtarget = p;
    }
    else
    {
        // not navigation and not filetype search
        switch (req_type)
        {
        case ST_EXACT:
            method = '=';
            break;

        case ST_S_SUBSTR:
            method = case_sen ? 'C' : 'S';
            break;

        case ST_S_REGEXP:
            method = 'R';
            if (!case_sen)
            {
                p = regexcase (newtarget);
                free (newtarget);
                newtarget = p;
            }
            break;

        case ST_S_GLOB:
            method = 'R';
            p = glob2regexp (newtarget, case_sen);
            free (newtarget);
            newtarget = p;
            break;

        case ST_M_SUBSTR:
            method = case_sen ? 'K' : 'Z';
            break;

        case ST_M_REGEXP:
            method = 'X';
            if (!case_sen)
            {
                p = regexcase (newtarget);
                free (newtarget);
                newtarget = p;
            }
            break;

        case ST_M_GLOB:
            method = 'X';
            p = glob2regexp (target, case_sen);
            free (newtarget);
            newtarget = p;
            break;
        }
    }

    if (method == '\0')
    {
        free (newtarget);
        return -1;
    }

    // 4. build search command
    p = str_strdup1 (newtarget, strlen(newtarget));
    free (newtarget);
    prospero_escape (p);
    snprintf1 (cmd, sizeof(cmd),
             "VERSION 5 query-1.0\n"
             "AUTHENTICATE '' UNAUTHENTICATED query-user\n"
             "DIRECTORY ASCII ARCHIE'/MATCH(%u,%u,%u,0,%c)/%s'\n",
             maxhits, maxhits, maxhits, method, p);
    free (p);

    if (path_inc != NULL && path_inc[0] == '\0') path_inc = NULL;
    if (path_exc != NULL && path_exc[0] == '\0') path_exc = NULL;
    if (dom_inc  != NULL && dom_inc[0]  == '\0') dom_inc = NULL;
    if (dom_exc  != NULL && dom_exc[0]  == '\0') dom_exc = NULL;

    if (path_inc == NULL && path_exc == NULL &&
        dom_inc == NULL && dom_exc == NULL && distrib_exclude == 0 &&
        start_date == 0 && end_date == 0 && min_size == 0 && max_size == 0)
    {
        strcat (cmd, "LIST ATTRIBUTES COMPONENTS\n");
        goto request_ready;
    }

    // Filters
    strcat (cmd, "LIST ATTRIBUTES+FILTER COMPONENTS\n");
    // path filters
    if (path_inc != NULL || path_exc != NULL || distrib_exclude)
    {
        strcat (cmd, "FILTER DIRECTORY SERVER PRE PREDEFINED AR_PATHCOMP ARGS ");
        skip_first_colon = 0;
        buf1[0] = '\0';
        if (path_inc != NULL)
        {
            skip_first_colon = 1;
            p1 = path_inc;
            do
            {
                p2 = strchr (p1, ' ');
                if (p2 != NULL) *p2 = '\0';
                if (!skip_first_colon) strcat (buf1, ":");
                //strcat (buf1, "!");
                strcat (buf1, p1);
                if (p2 != NULL) p1 = p2 + 1;
                skip_first_colon = 0;
            }
            while (p2 != NULL);
        }
        if (path_exc != NULL)
        {
            p1 = path_exc;
            do
            {
                p2 = strchr (p1, ' ');
                if (p2 != NULL) *p2 = '\0';
                if (!skip_first_colon) strcat (buf1, ":");
                strcat (buf1, "!");
                strcat (buf1, p1);
                if (p2 != NULL) p1 = p2 + 1;
                skip_first_colon = 0;
            }
            while (p2 != NULL);
        }
        if (distrib_exclude)
        {
            if (!skip_first_colon) strcat (buf1, ":");
            strcat (buf1, unix_distrib_excludes);
        }
        strcat (cmd, buf1);
        strcat (cmd, "\n");
    }

    // domain filters
    if (dom_inc != NULL || dom_exc != NULL)
    {
        strcat (cmd, "FILTER DIRECTORY SERVER PRE PREDEFINED AR_DOMAIN ARGS ");
        skip_first_colon = 0;
        buf1[0] = '\0';
        if (dom_inc != NULL)
        {
            skip_first_colon = 1;
            p1 = dom_inc;
            do
            {
                p2 = strchr (p1, ' ');
                if (p2 != NULL) *p2 = '\0';
                if (!skip_first_colon) strcat (buf1, ":");
                strcat (buf1, p1);
                if (p2 != NULL) p1 = p2 + 1;
                skip_first_colon = 0;
            }
            while (p2 != NULL);
        }
        if (dom_exc != NULL)
        {
            p1 = dom_exc;
            do
            {
                p2 = strchr (p1, ' ');
                if (p2 != NULL) *p2 = '\0';
                if (!skip_first_colon) strcat (buf1, ":");
                strcat (buf1, "!");
                strcat (buf1, p1);
                if (p2 != NULL) p1 = p2 + 1;
                skip_first_colon = 0;
            }
            while (p2 != NULL);
        }
        strcat (cmd, buf1);
        strcat (cmd, "\n");
    }

    // date/time filters
    if (start_date != 0 || end_date != 0)
    {
        strcat (cmd, "FILTER DIRECTORY SERVER PRE PREDEFINED DATE ARGS ");
        
        if (start_date != 0)
        {
            time1 = start_date;
            tm1 = *gmtime (&time1);
        }
        if (end_date != 0)
        {
            time1 = end_date;
            tm2 = *gmtime (&time1);
        }
        
        if (start_date != 0)
        {
            //start date
            snprintf1 (buffer, sizeof(buffer), "%04u%02u%02u%02u%02u%02uZ ",
                       tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
            strcat (cmd, buffer);
        }
        
        if (end_date != 0)
        {
            //end date
            //FILTER DIRECTORY SERVER PRE PREDEFINED DATE ARGS 0 19960101235959Z
            if (start_date == 0) strcat (cmd, "0 ");
            //start date and end date
            //FILTER DIRECTORY SERVER PRE PREDEFINED DATE ARGS 19960101000000Z 19990101235959Z
            snprintf1 (buffer, sizeof(buffer), "%04u%02u%02u%02u%02u%02uZ",
                     tm2.tm_year+1900, tm2.tm_mon+1, tm2.tm_mday, tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
            strcat (cmd, buffer);
        }
        strcat (cmd, "\n");
    }

    // size filters
    if (min_size != 0 || max_size != 0)
    {
        //size (only min)
        //FILTER DIRECTORY SERVER PRE PREDEFINED SIZE ARGS 10000 4294967295
        //size (min and max)
        //FILTER DIRECTORY SERVER PRE PREDEFINED SIZE ARGS 10000 100000
        strcat (cmd, "FILTER DIRECTORY SERVER PRE PREDEFINED SIZE ARGS ");
        if (max_size == 0) max_size = 4294967295UL;
        snprintf1 (buffer, sizeof(buffer), "%lu %lu", min_size, max_size);
        strcat (cmd, buffer);
        strcat (cmd, "\n");
    }

request_ready:

    //out = malloc (sizeof(Results));
    //out->n = 0;
    //out->hits = NULL;
    //out->request = strdup (cmd);
    //
    //return (int)out;
    debug_tools ("search command is: [%s]\n", cmd);

    if (RR == NULL) init_slots ();
    index = find_slot ();
    if (index == -1) return -1;

    RR[index].n = 0;
    RR[index].hits = NULL;
    RR[index].request = strdup (cmd);
    return index;
}

/* ====================================================================================== */

static char *regexcase (char *rx)
{
    char *p, *q, *result;
    int  escaped = FALSE;
    
    result = malloc (strlen (rx)*4+3);
    
    p = rx;
    q = result;
    
    while (*p)
    {
        if (*p == '[')
        {
            escaped = TRUE;
            *q++ = *p;
        }
        else if (*p == ']')
        {
            escaped = FALSE;
            *q++ = *p;
        }
        else if (!escaped && isalpha((unsigned)*p))
        {
            *q++ = '[';
            *q++ = tolower ((unsigned)*p);
            *q++ = toupper ((unsigned)*p);
            *q++ = ']';
        }
        else
        {
            *q++ = *p;
        }
        p++;
    }
    *q   = '\0';

    return result;
}

/* ====================================================================================== */

static char *glob2regexp (char *glob, int case_sen)
{
    char *p, *q, *result;

    result = malloc (strlen (glob)*4+3);
    q = result;
    p = glob;

    // start-of-line
    *q++ = '^';
    while (*p)
    {
        if (*p == '*')
        {
            *q++ = '.';
            *q++ = '*';
        }
        else if (*p == '?')
        {
            *q++ = '.';
        }
        else if (!isalnum((unsigned)*p))
        {
            *q++ = '\\';
            *q++ = *p;
        }
        else if (!case_sen && isalpha((unsigned)*p))
        {
            *q++ = '[';
            *q++ = tolower ((unsigned)*p);
            *q++ = toupper ((unsigned)*p);
            *q++ = ']';
        }
        else
        {
            *q++ = *p;
        }
        p++;
    }
    // end-of-line
    *q++ = '$';
    *q   = '\0';
    
    return result;
}

/* ======================================================================================

LINK L DIRECTORY NFTP INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/NFTP' 0
LINK L DIRECTORY nFTP INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/nFTP' 0
LINK L DIRECTORY nftp INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/nftp' 0
LINK L DIRECTORY KVNFTP INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/KVNFTP' 0
LINK L DIRECTORY anonftp INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/anonftp' 0
LINK L DIRECTORY seanftp INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/seanftp' 0

LINK L DIRECTORY nftp INTERNET-D crydee.sai.msu.ru(1525) ASCII ARCHIE'/HOST/crydee.sai.msu.ru/pub/comp/software/asv/nftp' 0
ATTRIBUTE CACHED INTRINSIC LAST-MODIFIED SEQUENCE 19981122202000Z
ATTRIBUTE CACHED INTRINSIC SIZE SEQUENCE 2048' bytes'
ATTRIBUTE CACHED INTRINSIC UNIX-MODES SEQUENCE drwxr-xr-x

LINK L EXTERNAL nftp.fls INTERNET-D crydee.sai.msu.ru ASCII '/pub/rec/games/solutions/unsorted/nftp.fls' 0
ATTRIBUTE CACHED FIELD ACCESS-METHOD AFTP '' '' '' '' BINARY
ATTRIBUTE CACHED INTRINSIC LAST-MODIFIED SEQUENCE 19950325003200Z
ATTRIBUTE CACHED INTRINSIC SIZE SEQUENCE 43116' bytes'
ATTRIBUTE CACHED INTRINSIC UNIX-MODES SEQUENCE -rw-r--r--
 */

int parse_prospero_response (char *filename, ftps_search_results **R)
{
    int         nr, nra;
    char        *p1, *p2, *H, *P, *F, buffer[8];
    char        line[1024];
    struct tm   t1;
    url_t       url;
    FILE        *fp;
    
    // check existence of file
    if (access (filename, R_OK) != 0) return -1;
    
    fp = fopen (filename, "r");
    if (fp == NULL) return -1;

    nra = 1024;
    nr  = -1;
    *R  = malloc (sizeof (ftps_search_results) * nra);

    while (TRUE)
    {
        if (fgets (line, sizeof(line), fp) == NULL) break;
        str_strip (line, "\n\r");
        
        // increase result buffer if necessary
        if (nr >= nra-1)
        {
            nra *= 2;
            *R = realloc (*R, sizeof (ftps_search_results) * nra);
        }
        
        H = NULL;
        P = NULL;
        F = NULL;
        // what is it?
        if (str_headcmp (line, "LINK L") == 0)
        {
            // LINK L EXTERNAL nftp.fls INTERNET-D crydee.sai.msu.ru ASCII '/pub/rec/games/solutions/unsorted/nftp.fls' 0
            // LINK L EXTERNAL 'Cure - A Letter To Elise.mp3' INTERNET-D ultra:mp3@161.184.88.207 ASCII '/mp3s/singles/C To D/Cure - A Letter To Elise.mp3' 0
            // LINK L EXTERNAL '(Cure) - Friday I''m In Love [Live].mp3' INTERNET-D look:look@24.65.40.35:2000 ASCII '/d:/download/Sorted/(Cure) - Friday I''m In Love [Live].mp3' 0
            if (str_headcmp (line, "LINK L EXTERNAL") == 0)
            {
                p1 = strstr (line, "INTERNET-D ");
                if (p1 == NULL) continue;
                p1 += strlen ("INTERNET-D ");
                p2 = strchr (p1, ' ');
                if (p2 == NULL) continue;
                *p2 = '\0';
                H = strdup (p1);
                p1 = strstr (p2+1, "ASCII '");
                if (p1 == NULL) continue;
                p1 += strlen ("ASCII '");
                p2 = strrchr (p1, '\'');
                if (p2 == NULL) continue;
                *p2 = '\0';
                // separate filename from path
                p2 = strrchr (p1, '/');
                if (p2 == NULL)  p2 = p1;
                else            *p2++ = '\0';
                P = strdup (*p1 == '\0' ? "/" : p1);
                F = strdup (p2);
            }
            else if (str_headcmp (line, "LINK L DIRECTORY") == 0)
            {
                if ((p1 = strstr (line, "ASCII ARCHIE'/HOST/")) != NULL)
                {
                    // LINK L DIRECTORY nftp INTERNET-D crydee.sai.msu.ru(1525) ASCII ARCHIE'/HOST/crydee.sai.msu.ru/pub/comp/software/asv/nftp' 0
                    // LINK L DIRECTORY 'Cure - Wild Mood Swings' INTERNET-D mp3.lycos.com(1525) ASCII ARCHIE'/HOST/mp3:mp3@208.220.225.122:2000/volumes_34-52/vol.44.mix/Cure - Wild Mood Swings' 0
                    p1 += strlen ("ASCII ARCHIE'/HOST/");
                    p2 = strchr (p1, '/');
                    if (p2 == NULL) continue;
                    *p2 = '\0';
                    H = strdup (p1);
                    *p2 = '/';
                    p1 = p2;
                    p2 = strrchr (p1, '\'');
                    if (p2 == NULL) continue;
                    *p2 = '\0';
                    // separate filename from path
                    p2 = strrchr (p1, '/');
                    if (p2 == NULL) p2 = p1;
                    else            *p2++ = '\0';
                    P = strdup (*p1 == '\0' ? "/" : p1);
                    F = strdup (p2);
                }
                else if ((p1 = strstr (line, "ASCII ARCHIE'/MATCH(")) != NULL)
                {
                    //LINK L DIRECTORY NFTP INTERNET-D sarth.sai.msu.ru(1525) ASCII ARCHIE'/MATCH(50,50,50,0,=)/NFTP' 0
                    p1 += strlen ("ASCII ARCHIE'/MATCH(");
                    p2 = strchr (p1, '/');
                    if (p2 == NULL) continue;
                    p2++;
                    p1 = strrchr (p2, '\'');
                    if (p1 == NULL) continue;
                    *p1 = '\0';
                    H = strdup (" ");
                    P = strdup (" ");
                    F = strdup (p2);
                }
            }
            else
            {
                // something weird...
                continue;
            }
            nr++;
            prospero_unescape (P);
            prospero_unescape (F);
            if (strchr (H, '@') != NULL || strchr (H, ':') != NULL)
            {
                parse_url (H, &url);
                (*R)[nr].hostname = strdup (url.hostname);
                (*R)[nr].userid = strdup (url.userid);
                (*R)[nr].password = strdup (url.password);
                (*R)[nr].port = url.port;
                free (H);
            }
            else
            {
                (*R)[nr].hostname = H;
                (*R)[nr].userid = NULL;
                (*R)[nr].password = NULL;
                (*R)[nr].port = 21;
            }
            (*R)[nr].pathname = P;
            (*R)[nr].filename = F;
            (*R)[nr].rights = 0;
            (*R)[nr].size = 0;
            (*R)[nr].timestamp = 0;
        }
        else
        {
            if (str_headcmp (line, "ATTRIBUTE CACHED INTRINSIC ") == 0)
            {
                if ((p1 = strstr (line, "SIZE SEQUENCE ")) != NULL)
                {
                    p1 += strlen ("SIZE SEQUENCE ");
                    p2 = strstr (p1, "' bytes'");
                    if (p2 == NULL) continue;
                    *p2 = '\0';
                    (*R)[nr].size = atol (p1);
                }
                else if ((p1 = strstr (line, "UNIX-MODES SEQUENCE ")) != NULL)
                {
                    p1 += strlen ("UNIX-MODES SEQUENCE ");
                    (*R)[nr].rights = perm_t2b (p1);
                }
                else if ((p1 = strstr (line, "LAST-MODIFIED SEQUENCE ")) != NULL)
                {
                    p1 += strlen ("LAST-MODIFIED SEQUENCE ");
                    if (strlen (p1) >= 14)
                    {
                        t1.tm_wday = 0;
                        t1.tm_yday = 0;
                        t1.tm_isdst = 0;
                        // 19950325 003200
                        memcpy (buffer, p1, 4);
                        buffer[4] = '\0';
                        t1.tm_year = atoi (buffer) - 1900;
                        memcpy (buffer, p1+4, 2);
                        buffer[2] = '\0';
                        t1.tm_mon = atoi (buffer) - 1;
                        memcpy (buffer, p1+6, 2);
                        t1.tm_mday = atoi (buffer);
                        memcpy (buffer, p1+8, 2);
                        t1.tm_hour = atoi (buffer);
                        memcpy (buffer, p1+10, 2);
                        t1.tm_min = atoi (buffer);
                        memcpy (buffer, p1+12, 2);
                        t1.tm_sec = atoi (buffer);
                        (*R)[nr].timestamp = timegm1 (&t1);
                    }
                }
                else
                {
                    // we don't know what it is...
                }
            }
        }
    }
    
    nr++;

    return nr;
}

/* ====================================================================================== */

static void prospero_unescape (char *s)
{
    str_replace (s, "''", "'", 0);
}

/* ====================================================================================== */

static void prospero_escape (char *s)
{
    char *p;
    
    // replace one ' with two ''
    p = s;
    while (*p)
    {
        if (*p == '\'')
        {
            str_insert (s, '\'', p-s);
            p++;
        }
        p++;
    }
}

/* ====================================================================================== */

/* Sorting weights */
int weights[6];
    
/* ====================================================================================== */

#define cm(a,b) ((a)==(b) ? 0 : ((a)>(b) ? 1 : -1))

int url_compare (__const__ void *elem1, __const__ void *elem2)
{
    ftps_search_results  *f1, *f2;
    int   w1, w2, w3, w4, w5, w6;
    char  *p1, *p2;

    f1 = (ftps_search_results *) elem1;
    f2 = (ftps_search_results *) elem2;

    // 1. Directories first, links follow them
    w1 = cm ((S_ISDIR (f2->rights) * 10 + S_ISLNK (f2->rights)),
             S_ISDIR (f1->rights) * 10 + S_ISLNK (f1->rights));

    // 2. File name
    w3 = cm (strcmp (f1->filename, f2->filename), 0);

    // 3. File size
    w4 = cm (f2->size, f1->size);

    // 4. Hostname
    p1 = strrchr (f1->hostname, '.');
    if (p1 == NULL) p1 = f1->hostname;
    else            p1++;
    p2 = strrchr (f2->hostname, '.');
    if (p2 == NULL) p2 = f2->hostname;
    else            p2++;
    w5 = cm (strcmp (p1, p2), 0);
    if (w5 == 0) w5 = cm (strcmp (f1->hostname, f2->hostname), 0);

    // 5. Time
    w2 = cm (f2->timestamp, f1->timestamp);

    // 6. Path
    w6 = cm (strcmp (f1->pathname, f2->pathname), 0);
    
    return (w1*weights[S_DIR] + w2*weights[S_TIME] + w3*weights[S_NAME] +
            w4*weights[S_SIZE] + w5*weights[S_HOST] + w6*weights[S_PATH]);
}

/* ====================================================================================== */

int FTPS_free (int search_handle)
{
    int i;

    for (i=0; i<RR[search_handle].n; i++)
    {
        free (RR[search_handle].hits[i].hostname);
        free (RR[search_handle].hits[i].pathname);
        free (RR[search_handle].hits[i].filename);
        if (RR[search_handle].hits[i].userid != NULL)   free (RR[search_handle].hits[i].userid);
        if (RR[search_handle].hits[i].password != NULL) free (RR[search_handle].hits[i].password);
    }
    if (RR[search_handle].hits != NULL) free (RR[search_handle].hits);
    if (RR[search_handle].request != NULL) free (RR[search_handle].request);
    free_slot (search_handle);

    return 0;
}

/* ====================================================================================== */

int FTPS_sort (int search_handle, int parm1, int parm2, int parm3, int parm4, int parm5)
{
    int  i;
    
    if (search_handle < 0) return -1;
    
    if (parm1 == -1 && parm2 == -1 && parm3 == -1 && parm4 == -1) return -1;

    // ignore everything
    for (i=0; i<6; i++) weights[i] = 0;

    if (parm1 != -1) weights[parm1] = 10000;
    if (parm2 != -1) weights[parm2] = 1000;
    if (parm3 != -1) weights[parm3] = 100;
    if (parm4 != -1) weights[parm4] = 10;
    if (parm5 != -1) weights[parm5] = 1;
    
    qsort (RR[search_handle].hits, RR[search_handle].n, sizeof(ftps_search_results), url_compare);
    
    return 0;
}

/* ====================================================================================== */

int FTPS_load (char *filename)
{
    int search_handle;

    if (RR == NULL) init_slots ();
    search_handle = find_slot ();
    if (search_handle < 0) return -1;

    RR[search_handle].n = 0;
    RR[search_handle].hits = NULL;
    RR[search_handle].request = NULL;
    
    RR[search_handle].n = parse_prospero_response (filename, &(RR[search_handle].hits));
    if (RR[search_handle].n == -1) return -1;

    return search_handle;
}

/* ====================================================================================== */

int FTPS_search (char *search_server, int search_port, int req, char *filename, int maxhits, double timeout)
{
    int             rc, sock, is_timeout, fh, n;
    double          time_target;
    char            *p;

    // check that server and search parameters are defined
    if (search_port == 0) search_port = 1525;
    if (search_server == NULL || req == -1) return -1;

    // open result buffer
    fh = open (filename, O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
    if (fh < 0) return -1;
    
    // create connection to server
    sock = ardp_open (search_server, search_port);
    if (sock < 0)
    {
        close (fh);
        return -1;
    }

    // send request to server
    rc = ardp_send (sock, RR[req].request);
    if (rc < 0)
    {
        ardp_close (sock);
        close (fh);
        return -1;
    }

    // wait for response packets and analyze them
    time_target = clock1() + timeout;
    do
    {
        rc = ardp_select (sock, 0.5);
        if (rc > 0)
        {
            n = ardp_receive (sock, &p);
            if (n < 0) break;
            if (n > 0)
            {
                write (fh, p, n);
            }
        }
        is_timeout = clock1() > time_target;
    }
    while (ardp_sequence () && !is_timeout);

    // close connection
    if (ardp_sequence()) ardp_cancel (sock, -1);
    ardp_close (sock);
    close (fh);

    RR[req].n = parse_prospero_response (filename, &(RR[req].hits));

    return 0;
}

/* ====================================================================================== */

int FTPS_num_results (int search_handle)
{
    if (search_handle < 0) return -1;
    if (slots[search_handle] == 0) return -1;
    
    return RR[search_handle].n;
}

/* ====================================================================================== */

ftps_search_results *FTPS_get_handle (int search_handle)
{
    if (search_handle < 0) return NULL;
    if (slots[search_handle] == 0) return NULL;

    return RR[search_handle].hits;
}

/* ====================================================================================== */

char *FTPS_get_request (int search_handle)
{
    if (search_handle < 0) return NULL;
    if (slots[search_handle] == 0) return NULL;

    return RR[search_handle].request;
}

/* ====================================================================================== */

int FTPS_set_num_results (int search_handle, int n1)
{
    if (search_handle < 0) return -1;
    if (slots[search_handle] == 0) return -1;
    
    RR[search_handle].n = n1;
    RR[search_handle].hits = realloc (RR[search_handle].hits, sizeof(RR[search_handle].hits[0]) * n1);
    return 0;
}
