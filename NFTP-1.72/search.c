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

static int sh;
static ftps_search_results *RS;

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
        nservers = 4;
    }
    else
    {
        // get from 'registry'
        for (i=0; i<nservers; i++)
        {
            snprintf1 (name, sizeof(name), "server-%u", i);
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
        snprintf1 (name, sizeof(name), "server-%u", i);
        snprintf1 (value, sizeof(value), "%u,%s", enabled[i], servers[i]);
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
    sh = FTPS_query (srch, target[10].value.integer, MT_FILE, method,
                     target[5].value.boolean,
                     target[9].value.boolean,
                     NULL, target[6].value.string,
                     target[7].value.string, target[8].value.string,
                     0, 0, 0, 0);
    if (sh < 0) goto ask_again;

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
        rc = ardp_send (sock, FTPS_get_request (sh));
        if (rc < 0)
        {
            ardp_close (sock);
            FTPS_free (sh);
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
                             insert_commas ((unsigned long)nres_t),
                             insert_commas ((unsigned long)nres));
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

    FTPS_free (sh);

    return ftpsearch_recall (u);
}

/* ------------------------------------------------------------- */

static int cmp_results (const void *e1, const void *e2)
{
    ftps_search_results *r1, *r2;
    int c;

    r1 = (ftps_search_results *) e1;
    r2 = (ftps_search_results *) e2;

    c = strcmp (r1->hostname, r2->hostname);
    if (c) return c;

    c = strcmp (r1->pathname, r2->pathname);
    if (c) return c;

    if (r1->size != r2->size) return r1->size - r2->size;

    return 0;
}

/* ------------------------------------------------------------- */

int  ftpsearch_recall (url_t *u)
{
    int   selection, i, n, n1;
    char  buffer[64], *s, *srchname;

    // open results file and load it
    srchname = str_strdup1 (paths.user_libpath, 12);
    str_cats (srchname, "nftp.srch");
    sh = FTPS_load (srchname);
    if (sh >= 0) n = FTPS_num_results (sh);
    if (sh < 0 || n == 0)
    {
        update (0);
        fly_ask_ok (0, sh < 0 ? MSG(M_FTPS_NORES) : MSG(M_FTPS_NO_HITS2));
        return -1;
    }

    RS = FTPS_get_handle (sh);

    // determine max. hostname length
    hostlenwidth = 0;
    for (i=0; i<n; i++)
    {
        hostlenwidth = max1 (hostlenwidth, strlen (RS[i].hostname) + numdigits (RS[i].size) + 1);
    }
    hostlenwidth = min1 (hostlenwidth, 40);

    // delete duplicate entries
    qsort (RS, n, sizeof(RS[0]), cmp_results);
    n1 = uniq2 (RS, n, sizeof(RS[0]), cmp_results);
    for (i=n1; i<n; i++)
    {
        free (RS[i].hostname);
        free (RS[i].pathname);
        free (RS[i].filename);
        if (RS[i].userid != NULL) free (RS[i].userid);
        if (RS[i].password != NULL) free (RS[i].password);
    }
    n = n1;
    /*
    for (i=0; i<n; i++)
    {
        for (j=i+1; j<n; j++)
        {
            if (RS[i].rights == RS[j].rights &&
                RS[i].size == RS[j].size &&
                strcmp (RS[i].hostname, RS[j].hostname) == 0 &&
                strcmp (RS[i].pathname, RS[j].pathname) == 0)
            {
                free (RS[j].hostname);
                free (RS[j].pathname);
                free (RS[j].filename);
                if (RS[j].userid != NULL) free (RS[j].userid);
                if (RS[j].password != NULL) free (RS[j].password);
                for (k=j+1; k<n; k++)
                {
                    RS[k-1] = RS[k];
                }
                n--;
            }
        }
    }
    */
    FTPS_set_num_results (sh, n);
    /* due to stupid API design FTP_set_num... reallocs RS!! */
    RS = FTPS_get_handle (sh);

    // sort results
    FTPS_sort (sh, S_DIR, S_NAME, -1, -1, -1);
    
    // Let user make a selection
    snprintf1 (buffer, sizeof(buffer), MSG(M_FTPS_SEARCHRES), n);
    s = video_save (0, 0, video_vsize()-1, video_hsize()-1);
    selection = fly_choose (buffer, CHOOSE_ALLOW_HSCROLL, &n, 0, findres_drawline, findres_callback);
    video_restore (s);
    
    // Assign results
    if (selection >= 0)
    {
        init_url (u);
        str_scopy (u->hostname, RS[selection].hostname);
        str_scopy (u->pathname, RS[selection].pathname);
        if (RS[selection].userid != NULL) strcpy (u->userid, RS[selection].userid);
        if (RS[selection].password != NULL) strcpy (u->password, RS[selection].password);
        u->port = RS[selection].port;
    }
    
    FTPS_free (sh);
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
    l1 = min1 (strlen (RS[n].hostname), hostlenwidth);
    video_put_str_attr (RS[n].hostname, l1, row, c0+1,
                   pointer ? options.attr_bmrk_hostpath_pointer : options.attr_bmrk_hostpath);

    // size
    if (S_ISDIR(RS[n].rights))      type = T_DIR;
    else if (S_ISLNK(RS[n].rights)) type = T_LNK;
    else if (S_ISREG(RS[n].rights)) type = T_REG;
    else                            type = T_OTH;
    
    switch (type)
    {
    case T_REG: p = pretty (RS[n].size); break;
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
        d = *gmtime (&(RS[n].timestamp));
        snprintf1 (buffer, sizeof(buffer), "%c %2u %3s %4u %02u:%02u", fl_sym.v,
                 d.tm_mday, mon2txt (d.tm_mon), d.tm_year+1900, d.tm_hour, d.tm_min);
        l1 = strlen (buffer);
        video_put_str (buffer, l1, row, c0);
        c0 += l1 + 1;
    }
    
    snprintf1 (buffer, sizeof(buffer), "%c ", fl_sym.v);
    video_put_str (buffer, 2, row, c0);
    c0 += 2;
    
    if (showpath)
    {
        p = str_strdup1 (RS[n].pathname, strlen(RS[n].filename)+2);
        str_cats (p, RS[n].filename);
    }
    else
    {
        p = strdup (RS[n].filename);
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
                if (str_stristr (RS[i].hostname, buffer) != NULL ||
                    str_stristr (RS[i].pathname, buffer) != NULL) break;
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
                if (str_stristr (RS[i].hostname, buffer) != NULL ||
                    str_stristr (RS[i].pathname, buffer) != NULL) break;
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
            if (!S_ISREG (RS[*n].rights)) break;
            if (fly_ask (ASK_YN, MSG(M_FTPS_DOWNLOAD), NULL, RS[*n].pathname, RS[*n].hostname))
            {
                snprintf1 (buf2, sizeof(buf2), "ftp://%s%s", RS[*n].hostname, RS[*n].pathname);
                do_get (buf2);
                update (0);
            }
            break;

        case 'T':
            FTPS_sort (sh, S_DIR, S_TIME, -1, -1, -1);
            *n = 0;
            break;
            
        case 'S':
            FTPS_sort (sh, S_DIR, S_SIZE, -1, -1, -1);
            *n = 0;
            break;
            
        case 'N':
            FTPS_sort (sh, S_DIR, S_NAME, -1, -1, -1);
            *n = 0;
            break;
            
        case 'H':
            FTPS_sort (sh, S_HOST, -1, -1, -1, -1);
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
