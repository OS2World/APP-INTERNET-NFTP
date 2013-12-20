#include <fly/fly.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>
#include <fcntl.h>

#include "nftp.h"
#include "auxprogs.h"
#include "search.h"
#include "network.h"
//#include "ardp.h"
#include <fsearch.h>

#ifdef __EMX__
#include <sys/select.h>
#endif

#define MAX_FTPSEARCH_SERVERS   64

#define RR ((Results *)R)
static int R;

static int   nservers = 0;
static char  *servers[MAX_FTPSEARCH_SERVERS], enabled[MAX_FTPSEARCH_SERVERS];
static int   changed = FALSE, loaded = FALSE;

static int  showpath = FALSE, showtime = FALSE;
static int  hostlenwidth = 0;

static void servers_drawline (int n, int len, int shift, int pointer, int row, int col);
static int  servers_callback (int *nchoices, int *n, int key);
static void findres_drawline (int n, int len, int shift, int pointer, int row, int col);
static int  findres_callback (int *nchoices, int *n, int key);
int numdigits (unsigned long n);

/* ------------------------------------------------------------- */

int ftpsearch_servers (void)
{
    int   rc;
    char  *s;

    if (!loaded) ftpsearch_load_servers ();

    s = video_save (0, 0, video_vsize()-1, video_hsize()-1);
    rc = fly_choose (MSG(M_FTPS_SERVERS), 0, &nservers, 0, servers_drawline, servers_callback);
    video_restore (s);

    if (rc == RC_CHOOSE_CANCELLED) ftpsearch_load_servers ();
    if (rc != RC_CHOOSE_CANCELLED && changed) ftpsearch_save_servers ();
    if (rc == RC_CHOOSE_CANCELLED) return -1;
    return 0;
}

/* ------------------------------------------------------------- */

static void servers_drawline (int n, int len, int shift, int pointer, int row, int col)
{
    video_put_n_char (' ', len, row, col);
    video_put (" [ ] ", row, col);
    if (enabled[n]) video_put_n_char ('X', 1, row, col+2);
    video_put_str (servers[n], min1 (len-5, strlen(servers[n])), row, col+5);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}

/* ------------------------------------------------------------- */

static int servers_callback (int *nchoices, int *n, int key)
{
    int    rc, i;
    char   srv[128];

    if (IS_KEY (key))
    {
        switch (key)
        {
        case _Space:
            if (enabled[*n])
                enabled[*n] = 0;
            else
                enabled[*n] = 1;
            changed = TRUE;
            break;

        case _Insert:
            srv[0] = '\0';
            rc = entryfield (MSG(M_FTPS_ADD_SERVER), srv, sizeof (srv), 0);
            if (rc == PRESSED_ENTER)
            {
                servers[nservers] = strdup (srv);
                enabled[nservers] = TRUE;
                nservers++;
                changed = TRUE;
                *n = nservers-1;
            }
            break;

        case _Delete:
            rc = fly_ask (ASK_YN, MSG(M_FTPS_DELETE), NULL, servers[*n]);
            if (rc == 1)
            {
                free (servers[*n]);
                for (i=*n; i<nservers-1; i++)
                {
                    servers[i] = servers[i+1];
                    enabled[i] = enabled[i+1];
                }
                nservers--;
                changed = TRUE;
            }
            break;
        }
    }

    if (IS_MOUSE (key))
    {
        if (MOU_X(key) < 5 || MOU_X(key) > video_hsize()-5) return -2;
        i = MOU_Y(key) - 3;
        if (i < 0 || i > *nchoices-1) return -2;
        switch (MOU_EVTYPE(key))
        {
        case MOUEV_B1SC:
            enabled[i] = !enabled[i];
            changed = TRUE;
            break;
        case MOUEV_B1DC:
            return -3;
        }
    }

    return -2;
}

/* ------------------------------------------------------------- */

void ftpsearch_load_servers (void)
{
    char   *p, *p1;
    char   name[32];
    int    i;

    nservers = cfg_get_integer (CONFIG_NFTP, "ftpsearch", "num-servers");
    if (nservers == 0)
    {
        servers[0] = "ftpsearch.lycos.com";               enabled[0] = TRUE;
        servers[1] = "mp3.lycos.com";                     enabled[1] = FALSE;
        servers[2] = "ftpsearch.rambler.ru";              enabled[2] = TRUE;
        servers[3] = "www.filesearch.ru";                 enabled[3] = FALSE;
        changed = TRUE;
        nservers = 3;
    }
    else
    {
        // get from 'registry'
        for (i=0; i<nservers; i++)
        {
            sprintf (name, "server-%u", i);
            p = cfg_get_string (CONFIG_NFTP, "ftpsearch", name);
            servers[i] = "unknown";
            enabled[i] = 0;
            if (p == NULL || p[0] == '\0') continue;
            p = strdup (p);
            p1 = strchr (p, ',');
            if (p1 == NULL) continue;
            *p1 = '\0';
            enabled[i] = atoi (p);
            servers[i] = strdup (p1+1);
            free (p);
        }
    }
}

/* ------------------------------------------------------------- */

void ftpsearch_save_servers (void)
{
    char   name[32], value[1024];
    int    i;

    // put info into 'registry'
    cfg_set_integer (CONFIG_NFTP, "ftpsearch", "num-servers", nservers);
    for (i=0; i<nservers; i++)
    {
        sprintf (name, "server-%u", i);
        sprintf (value, "%u,%s", enabled[i], servers[i]);
        cfg_set_string (CONFIG_NFTP, "ftpsearch", name, value);
    }
}

/* ------------------------------------------------------------- */

int  ftpsearch_find (char *s, url_t *u)
{
    int             i, reply, rc, sock, n;
    int             fd, nres_t, nres, key;
    entryfield_t    target[11];
    char            method;
    char            *p, *srch, *wt, *srchname;

    // 1. ask about servers if needed
    if (!loaded) ftpsearch_load_servers ();
    if (nservers == 0 || options.ftps_show_servers)
    {
        rc = ftpsearch_servers ();
        if (rc < 0) return -1;
    }

    // 2. Ask user what is to look for
    target[0].banner = MSG(M_FTPS_SEARCH1);
    target[0].type = EF_STRING;
    str_scopy (target[0].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-exact"));
    target[0].flags = 0;

    target[1].banner = MSG(M_FTPS_SEARCH2);
    target[1].type = EF_STRING;
    str_scopy (target[1].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-substring"));
    target[1].flags = 0;

    target[2].banner = MSG(M_FTPS_SEARCH3);
    target[2].type = EF_STRING;
    str_scopy (target[2].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-wildcard"));
    target[2].flags = 0;

    target[3].banner = MSG(M_FTPS_SEARCH4);
    target[3].type = EF_STRING;
    str_scopy (target[3].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-regex"));
    target[3].flags = 0;

    target[4].type = EF_SEPARATOR;

    target[5].banner = MSG(M_FTPS_SEARCH5);
    target[5].type = EF_BOOLEAN;
    target[5].value.boolean = cfg_get_boolean (CONFIG_NFTP, "ftpsearch", "last-casesensitive");
    target[5].flags = 0;

    target[6].banner = MSG(M_FTPS_SEARCH6);
    target[6].type = EF_STRING;
    str_scopy (target[6].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-excludepath"));
    target[6].flags = 0;

    target[7].banner = MSG(M_FTPS_SEARCH7);
    target[7].type = EF_STRING;
    str_scopy (target[7].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-limitdomain"));
    target[7].flags = 0;

    target[8].banner = MSG(M_FTPS_SEARCH8);
    target[8].type = EF_STRING;
    str_scopy (target[8].value.string, cfg_get_string (CONFIG_NFTP, "ftpsearch", "last-excludedomain"));
    target[8].flags = 0;

    target[9].banner = MSG(M_FTPS_SEARCH9);
    target[9].type = EF_BOOLEAN;
    target[9].value.boolean = cfg_get_boolean (CONFIG_NFTP, "ftpsearch", "last-hidepackages");
    target[9].flags = 0;

    target[10].banner = MSG(M_FTPS_SEARCH10);
    target[10].type = EF_INTEGER;
    target[10].value.integer = cfg_get_integer (CONFIG_NFTP, "ftpsearch", "last-maxhits");
    if (target[10].value.integer <= 0) target[10].value.integer = 128;
    target[10].flags = 0;

    if (s != NULL && s[0] != '\0')
    {
        str_scopy (target[0].value.string, s);
        target[1].value.string[0] = '\0';
        target[2].value.string[0] = '\0';
        target[3].value.string[0] = '\0';
    }

ask_again:
    reply = mle2 (11, target, MSG(M_USETAB2), M_HLP_FTPSEARCH);
    if (reply != PRESSED_ENTER) return -1;

    // check that only one of substring/glob/regex is specified
    str_strip2 (target[0].value.string, " ");
    str_strip2 (target[1].value.string, " ");
    str_strip2 (target[2].value.string, " ");
    str_strip2 (target[3].value.string, " ");
    n = 0;
    method = ST_S_SUBSTR;
    srch = target[0].value.string;
    if (target[0].value.string[0] != '\0')
    {
        n++;
        method = ST_EXACT;
        srch = target[0].value.string;
    }
    if (target[1].value.string[0] != '\0')
    {
        n++;
        method = ST_S_SUBSTR;
        srch = target[1].value.string;
    }
    if (target[2].value.string[0] != '\0')
    {
        n++;
        method = ST_S_GLOB;
        srch = target[2].value.string;
    }
    if (target[3].value.string[0] != '\0')
    {
        n++;
        method = ST_S_REGEXP;
        srch = target[3].value.string;
    }
    if (n == 0 || n > 1)
    {
        fly_ask_ok (0, MSG(M_FTPS_NEED_ARGS));
        goto ask_again;
    }

    // 3. save search parameters
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-exact",         target[0].value.string);
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-substring",     target[1].value.string);
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-wildcard",      target[2].value.string);
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-regex",         target[3].value.string);
    cfg_set_boolean (CONFIG_NFTP, "ftpsearch", "last-casesensitive", target[5].value.boolean);
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-excludepath",   target[6].value.string);
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-limitdomain",   target[7].value.string);
    cfg_set_string  (CONFIG_NFTP, "ftpsearch", "last-excludedomain", target[8].value.string);
    cfg_set_boolean (CONFIG_NFTP, "ftpsearch", "last-hidepackages",  target[9].value.boolean);
    cfg_set_integer (CONFIG_NFTP, "ftpsearch", "last-maxhits",       target[10].value.integer);
    write_config ();

    str_strip2 (target[6].value.string, " ");
    str_strip2 (target[7].value.string, " ");
    str_strip2 (target[8].value.string, " ");

    // path exclusions
    R = FTPS_query (srch, target[10].value.integer, 0, method,
                    target[5].value.boolean,
                    target[9].value.boolean,
                    NULL, target[6].value.string,
                    target[7].value.string, target[8].value.string,
                    0, 0, 0, 0);
    if (R == 0) goto ask_again;

    // 5. open file to write results
    srchname = str_strdup1 (paths.user_libpath, 12);
    str_cats (srchname, "nftp.srch");
    fd = open (srchname, O_WRONLY|O_CREAT|O_TRUNC|BINMODE, 0666);
    if (fd < 0)
    {
        fly_ask_ok (ASK_WARN, MSG(M_CANT_OPEN_FOR_WRITING), srchname);
        return -1;
    }

    // 6. Process each server in turn
    wt = get_window_name ();
    nres_t = 0;
    for (i=0; i<nservers; i++)
    {
        if (!enabled[i]) continue;

        nres = 0;
        if (fl_opt.is_unix)
            set_window_name ("querying %s...", servers[i]);
        else
            set_window_name (MSG(M_QUERYING), servers[i]);

        put_message (MSG(M_FTPS_SENDING_REQUEST), servers[i]);

        // create connection to server
        sock = ardp_open (servers[i], 1525);
        if (sock < 0) return -1;

        // send request to server
        rc = ardp_send (sock, RR->request);
        if (rc < 0)
        {
            ardp_close (sock);
            return -1;
        }

        do
        {
            // wait for response packets
            rc = ardp_select (sock, 0.3);
            if (rc < 0) break;
            if (rc > 0)
            {
                // receive data store it
                n = ardp_receive (sock, &p);
                if (n < 0) break;
                if (n > 0)
                {
                    write (fd, p, n);
                    n = str_numstr (p, " INTERNET-D ");
                    nres_t += n;
                    nres   += n;
                }
                put_message (MSG(M_FTPS_RECEIVING2), servers[i],
                             pretty ((unsigned long)nres_t),
                             pretty ((unsigned long)nres));
            }

            // if the user has pressed Esc, exit
            do
            {
                key = getkey (0);
                if (key == _Esc) goto finish;
            }
            while (key);
        }
        while (ardp_sequence ());

    finish:
        if (ardp_sequence()) ardp_cancel (sock, -1);
        ardp_close (sock);
    }
    close (fd);
    set_window_name (wt);
    free (wt);

    FTPS_free (R);

    return ftpsearch_recall (u);
}

/* ------------------------------------------------------------- */

int  ftpsearch_recall (url_t *u)
{
    int   selection, i, j, k;
    char  buffer[64], *s, *srchname;

    // open results file and load it
    srchname = str_strdup1 (paths.user_libpath, 12);
    str_cats (srchname, "nftp.srch");
    R = FTPS_load (srchname);
    if (R == -1 || RR->n == 0)
    {
        update (0);
        fly_ask_ok (0, R == -1 ? MSG(M_FTPS_NORES) : MSG(M_FTPS_NO_HITS2));
        return -1;
    }

    // determine max. hostname length
    hostlenwidth = 0;
    for (i=0; i<RR->n; i++)
    {
        hostlenwidth = max1 (hostlenwidth, strlen (RR->hits[i].hostname) + numdigits (RR->hits[i].size) + 1);
    }
    hostlenwidth = min1 (hostlenwidth, 40);

    // delete duplicate entries
    for (i=0; i<RR->n; i++)
    {
        for (j=i+1; j<RR->n; j++)
        {
            if (RR->hits[i].rights == RR->hits[j].rights &&
                RR->hits[i].size == RR->hits[j].size &&
                strcmp (RR->hits[i].hostname, RR->hits[j].hostname) == 0 &&
                strcmp (RR->hits[i].pathname, RR->hits[j].pathname) == 0)
            {
                free (RR->hits[j].hostname);
                free (RR->hits[j].pathname);
                free (RR->hits[j].filename);
                if (RR->hits[j].userid != NULL) free (RR->hits[j].userid);
                if (RR->hits[j].password != NULL) free (RR->hits[j].password);
                for (k=j+1; k<RR->n; k++)
                {
                    RR->hits[k-1] = RR->hits[k];
                }
                RR->n--;
            }
        }
    }

    // sort results
    FTPS_sort (R, S_DIR, S_NAME, -1, -1, -1);

    // Let user make a selection
    sprintf (buffer, MSG(M_FTPS_SEARCHRES), RR->n);
    s = video_save (0, 0, video_vsize()-1, video_hsize()-1);
    selection = fly_choose (buffer, CHOOSE_ALLOW_HSCROLL, &RR->n, 0, findres_drawline, findres_callback);
    video_restore (s);

    // Assign results
    if (selection >= 0)
    {
        init_url (u);
        str_scopy (u->hostname, RR->hits[selection].hostname);
        str_scopy (u->pathname, RR->hits[selection].pathname);
        if (RR->hits[selection].userid != NULL) strcpy (u->userid, RR->hits[selection].userid);
        if (RR->hits[selection].password != NULL) strcpy (u->password, RR->hits[selection].password);
        u->port = RR->hits[selection].port;
    }

    FTPS_free (R);
    update (0);
    return selection >= 0 ? 0 : -1;
}

/* ------------------------------------------------------------- */

static void findres_drawline (int n, int len, int shift, int pointer, int row, int col)
{
    int        l1, l2, c0, type;
    char       *p, buffer[64];
    struct tm   d;

    c0 = col;
    video_put_n_char (' ', len, row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);

    // hostname
    l1 = min1 (strlen (RR->hits[n].hostname), hostlenwidth);
    video_put_str_attr (RR->hits[n].hostname, l1, row, c0+1,
                   pointer ? options.attr_bmrk_hostpath_pointer : options.attr_bmrk_hostpath);

    // size
    if (S_ISDIR(RR->hits[n].rights))      type = T_DIR;
    else if (S_ISLNK(RR->hits[n].rights)) type = T_LNK;
    else if (S_ISREG(RR->hits[n].rights)) type = T_REG;
    else                             type = T_OTH;

    switch (type)
    {
    case T_REG: p = pretty (RR->hits[n].size); break;
    case T_DIR: p = MSG(M_DIR_SIGN); break;
    case T_LNK: p = MSG(M_LINK_SIGN); break;
    default:
    case T_OTH: p = "?????"; break;
    }
    l1 = strlen (p);
    video_put_str (p, l1, row, c0+hostlenwidth-l1+1);
    c0 += hostlenwidth+2;

    if (showtime)
    {
        d = *gmtime (&(RR->hits[n].timestamp));
        sprintf (buffer, "%c %2u %3s %4u %02u:%02u", fl_sym.v,
                 d.tm_mday, mon2txt (d.tm_mon), d.tm_year+1900, d.tm_hour, d.tm_min);
        l1 = strlen (buffer);
        video_put_str (buffer, l1, row, c0);
        c0 += l1 + 1;
    }

    sprintf (buffer, "%c ", fl_sym.v);
    video_put_str (buffer, 2, row, c0);
    c0 += 2;

    if (showpath)
    {
        p = str_strdup1 (RR->hits[n].pathname, strlen(RR->hits[n].filename)+2);
        str_cats (p, RR->hits[n].filename);
    }
    else
    {
        p = strdup (RR->hits[n].filename);
    }

    if (strlen (p) > shift)
    {
        l2 = min1 (strlen (p)-shift, col+len-c0);
        video_put_str (p+shift, l2, row, c0);
    }
    free (p);
}

/* ------------------------------------------------------------- */

static int findres_callback (int *nchoices, int *n, int key)
{
    static char buffer[1024] = "";
    int         rc, i;
    char        buf2[1024];

    if (IS_KEY(key))
    {
        switch (key)
        {
        case _F1:
        case '?':
            help (M_HLP_FTPSEARCH2);
            break;

        case _CtrlF:
        ask_for_target:
            buffer[0] = '\0';
            rc = entryfield (MSG(M_SEARCH_FORWARD), buffer, sizeof (buffer), 0);
            if (rc != PRESSED_ENTER || buffer[0] == '\0') break;

        case _CtrlG:
            if (buffer[0] == '\0') goto ask_for_target;
            for (i=*n+1; i<*nchoices; i++)
            {
                if (str_stristr (RR->hits[i].hostname, buffer) != NULL ||
                    str_stristr (RR->hits[i].pathname, buffer) != NULL) break;
            }
            if (i == *nchoices)
                fly_ask_ok (0, MSG(M_CANT_FIND), buffer);
            else
                *n = i;
            break;

        case _CtrlH:
            if (buffer[0] == '\0')
            {
                buffer[0] = '\0';
                rc = entryfield (MSG(M_SEARCH_BACKWARDS), buffer, sizeof (buffer), 0);
                if (rc != PRESSED_ENTER || buffer[0] == '\0') break;
            }
            for (i=max1(*n-1, 0); i>=0; i--)
            {
                if (str_stristr (RR->hits[i].hostname, buffer) != NULL ||
                    str_stristr (RR->hits[i].pathname, buffer) != NULL) break;
            }
            if (i == *nchoices)
                fly_ask_ok (0, MSG(M_CANT_FIND), buffer);
            else
                *n = i;
            break;

        case _CtrlP:
            if (showpath) showpath = FALSE;
            else          showpath = TRUE;
            dmsg ("showpath is now %d\n", showpath);
            break;

        case _CtrlT:
            if (showtime) showtime = FALSE;
            else          showtime = TRUE;
            break;

        case 'D':
        case _F5:
            if (!S_ISREG (RR->hits[*n].rights)) break;
            if (fly_ask (ASK_YN, MSG(M_FTPS_DOWNLOAD), NULL, RR->hits[*n].pathname, RR->hits[*n].hostname))
            {
                sprintf (buf2, "ftp://%s%s", RR->hits[*n].hostname, RR->hits[*n].pathname);
                do_get (buf2);
                update (0);
            }
            break;

        case 'T':
            FTPS_sort (R, S_DIR, S_TIME, -1, -1, -1);
            *n = 0;
            break;

        case 'S':
            FTPS_sort (R, S_DIR, S_SIZE, -1, -1, -1);
            *n = 0;
            break;

        case 'N':
            FTPS_sort (R, S_DIR, S_NAME, -1, -1, -1);
            *n = 0;
            break;

        case 'H':
            FTPS_sort (R, S_HOST, -1, -1, -1, -1);
            *n = 0;
            break;
        }
    }
    return -2;
}

/* ------------------------------------------------------------- */

int numdigits (unsigned long n)
{
    if (n < 10)             return 1;  // 8
    if (n < 100)            return 2;  // 12
    if (n < 1000)           return 3;  // 678
    if (n < 10000)          return 5;  // 6,567
    if (n < 100000)         return 6;  // 56,567
    if (n < 1000000)        return 7;  // 345,456
    if (n < 10000000)       return 9;  // 4,345,456
    if (n < 100000000)      return 10; // 14,345,456
    if (n < 1000000000)     return 11; // 514,345,456
    return 13;
}
