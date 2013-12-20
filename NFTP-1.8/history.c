#include <fly/fly.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "nftp.h"
#include "net.h"
#include "history.h"
#include "auxprogs.h"
#include "network.h"
#include "passwords.h"

/* File format:
 time,host,IP,port,directory,username,password(scrambled),flags */

/* dimensions */

#define MAX_HIST_ENTRIES  ((options.history_entries_watermark*15)/10)
#define MAX_HIST_SITES    ((options.history_sites_watermark*15)/10)
#define MAX_HIST_PERSITE  ((options.history_persite_watermark*15)/10)

/* flags */

#define HIST_DELETED 0x10000000

struct entry_t
{
    time_t         tm; // time when the mark was stored
    char           *hostname; // hostname
    char           *userid; // userid ("" for anonymous)
    char           *directory; // directory on server; can't be NULL
    unsigned long  ip; // IP address of server
    unsigned       port; // port to connect
    int            flags; // various flags
    int            hash;
};

struct site_t
{
    unsigned long  ip; // IP address of server
    char           *hostname;
    int            *n; // indexes in entry_t array
    int            N;
};

//static struct entry_t cached = {0L, NULL, NULL, NULL, 0L, 0, 0};
static struct entry_t *hist;
static struct site_t *sites;
static int nhist, nsites, changed;
static void *chunks = NULL;

static int   f_fline, f_cursor, view, selection;

enum {ORDER_HOSTNAME, ORDER_TIME};
static int   hist_sort_order;

void history_write_line
    (FILE *hf, time_t tm, char *hostname, unsigned long ip,
     char *userid, int port, int flags, char *directory);

int history_read_line
    (FILE *hf, time_t *timestamp, char *hostname, unsigned long *ip,
     char *userid, unsigned int *port, int *flags, char *directory);

void draw_hist_line (int n, int len, int shift, int pointer, int row, int col);

int  history_read (int clean);
int  history_write (void);
void history_do_clean (void);

static int same_site (int n1, int n2);
/*static void history_dump (void);*/
static void history_add_site (int i);
static void history_free (void);

static int hist_compare (__const__ void *elem1, __const__ void *elem2);

void drawline_site (int n, int len, int shift, int pointer, int row, int col);
void drawline_dir (int n, int len, int shift, int pointer, int row, int col);

int  callback_site (int *nchoices, int *n, int key);
int  callback_dir  (int *nchoices, int *n, int key);

/* -------------------------------------------------------------------
 appends line to history file. returns 0 if no error, 1 if failed
 */
int history_add (url_t *u)
{
    FILE   *hf;
    char   *p;

    if (!options.history_active) return 0;

    // open history file
    hf = try_open (options.history_file, "a");
    if (hf == NULL) return 1;

    p = strdup (u->pathname);
    if (str_tailcmp (p, "/") == 0 &&
        (strlen(p) != 1 && !(strlen(p) == 3 && p[1] == ':')))
    {
        str_strip (p, "/");
    }
    history_write_line (hf, gm2local(time(NULL)), u->hostname, u->ip,
                        u->userid, u->port, u->flags, p);
    free (p);

    fclose (hf);

    return 0;
}

/* -------------------------------------------------------------------
 */
void history_do_clean (void)
{
    int           i, j;
    double        ts, te, t1, t2;
    unsigned char *p, c;

    dmsg ("start cleaning\n");
    ts = clock1 ();

    // compute hash
    t1 = clock1 ();
    for (i=0; i<nhist; i++)
    {
        hist[i].hash = 0;
        p = (unsigned char *)hist[i].directory;
        do
        {
            c = *p;
            hist[i].hash += c;
            p++;
        }
        while (c);
    }
    t2 = clock1();
    dmsg ("computing hash: %f s\n", t2-t1);

    // delete entries which have no directory name
    if (!options.slowlink)
        put_message (M("Processing history; stage %u..."), 1);
    t1 = clock1 ();
    for (i=0; i<nhist; i++)
    {
        if (!options.slowlink && i !=0 && i % 1000 == 0)
            put_message (M("Processing history; stage %u:  %u entries done"), 1, i);
        if (hist[i].flags & HIST_DELETED) continue;
        if (hist[i].directory[0] != '\0') continue;
        for (j=0; j<nhist; j++)
        {
            if (hist[j].directory[0] != '\0' && same_site (i, j))
            {
                hist[i].flags |= HIST_DELETED;
                break;
            }
        }
    }
    t2 = clock1();
    dmsg ("deleting entries without dir. name: %f s\n", t2-t1);

    // sort by dirname (i.e. hash)
    hist_sort_order = ORDER_HOSTNAME;
    t1 = clock1 ();
    qsort (hist, nhist, sizeof(hist[0]), hist_compare);
    t2 = clock1();
    dmsg ("sorting by dirname: %f s\n", clock1()-ts);

    // delete identical entries
    if (!options.slowlink)
        put_message (M("Processing history; stage %u..."), 2);
    t1 = clock1 ();
    for (i=0; i<nhist; i++)
    {
        if (hist[i].flags & HIST_DELETED) continue;
        for (j=i+1; j<nhist; j++)
        {
            //if (strcmp (hist[i].directory, hist[j].directory)) break;
            if (hist[i].hash != hist[j].hash) break;
            if (hist[j].flags & HIST_DELETED) continue;
            if (same_site (i, j))
            {
                hist[j].flags |= HIST_DELETED;
            }
        }
    }
    t2 = clock1();
    dmsg ("deleting duplicates: %f s\n", t2-t1);

    t1 = clock1 ();
    hist_sort_order = ORDER_TIME;
    qsort (hist, nhist, sizeof(hist[0]), hist_compare);
    t2 = clock1();
    dmsg ("sorting by time: %f s\n", t2-t1);

    if (!options.slowlink)
        put_message (M("Processing history; stage %u..."), 3);

    // create groups by IP address
    t1 = clock1 ();
    nsites = 0;
    for (i=nhist-1; i>=0; i--)
        if (!(hist[i].flags & HIST_DELETED))
            history_add_site (i);
    t2 = clock1();
    dmsg ("creating sites: %f s\n", t2-t1);

    // checking history_sites_watermark
    t1 = clock1 ();
    while (nsites > options.history_sites_watermark)
    {
        for (i=0; i<sites[nsites-1].N; i++)
            hist[sites[nsites-1].n[i]].flags |= HIST_DELETED;
        nsites--;
    }
    t2 = clock1();
    dmsg ("checking sites watermark: %f s\n", t2-t1);

    // checking history_persite_watermark
    t1 = clock1 ();
    for (i=0; i<nsites; i++)
    {
        while (sites[i].N > options.history_persite_watermark)
        {
            hist[sites[i].n[sites[i].N-1]].flags |= HIST_DELETED;
            sites[i].N--;
        }
    }
    t2 = clock1();
    dmsg ("checking per-site watermark: %f s\n", t2-t1);

    te = clock1 ();
    dmsg ("finished cleaning on %d items; elapsed time = %f s\n", nhist, te-ts);
}

/* -------------------------------------------------------------------
 */
static int same_site (int n1, int n2)
{
    if (hist[n1].ip == hist[n2].ip && hist[n1].ip != 0) return 1;
    if (strcmp (hist[n1].hostname, hist[n2].hostname) == 0) return 1;
    return 0;
}

/* -------------------------------------------------------------------
 */
int history_read (int clean)
{
    FILE   *hf;
    int    i, rc;
    char   hostname[64], userid[64], directory[1024];
    double t1, t2, t3;

    dmsg ("history_read() entered\n", options.history_file);

    nhist  = 0;
    nsites = 0;
    if (access (options.history_file, R_OK) != 0) return 0;

    // allocate storage
    hist = malloc (sizeof (struct entry_t)*MAX_HIST_ENTRIES);
    if (hist == NULL) return 1;
    sites = malloc (sizeof (struct site_t)*MAX_HIST_SITES);
    if (sites == NULL) return 1;
    if (chunks != NULL) chunk_free (chunks);
    chunks = chunk_new (0);

    // open history file and read it
    dmsg ("opening history file %s\n", options.history_file);
    hf = try_open (options.history_file, "r");
    if (hf == NULL) return 1;
    dmsg ("opened history\n");
    t2 = 0;
    t3 = 0;
    for (i=0; i<MAX_HIST_ENTRIES; i++)
    {
        t1 = clock1 ();
        rc = history_read_line
            (hf, &(hist[i].tm), hostname, &(hist[i].ip),
             userid, &(hist[i].port), &(hist[i].flags),
             directory);
        if (rc != 0) break;
        t2 += clock1 () - t1;
        t1 = clock1 ();
        hist[i].hostname = chunk_put (chunks, hostname, -1);
        hist[i].userid = chunk_put (chunks, userid, -1);
        hist[i].directory = chunk_put (chunks, directory, -1);
        if (!options.slowlink && i != 0 && i % 1000 == 0 && clean)
            put_message (M("Reading history file;  %u lines done"), i);
        t3 += clock1 () - t1;
    }
    nhist = i;
    fclose (hf);
    dmsg ("done reading history; %d lines, %f sec in history_read_line(), %f sec rest\n",
          nhist, t2, t3);

    if (clean)
        history_do_clean ();

    dmsg ("done cleaning history\n");
    /*history_dump ();*/
    return 0;
}

/* -------------------------------------------------------------------
 */

static void history_add_site (int i)
{
    int  j;

    // look whether this group already exists
    for (j=0; j<nsites; j++)
    {
        if ((sites[j].ip == hist[i].ip && sites[j].ip != 0) ||
           strcmp(sites[j].hostname, hist[i].hostname) == 0) break;
    }
    // not found; adding new group
    if (j==nsites)
    {
        if (nsites == MAX_HIST_SITES-1)
        {
            hist[i].flags |= HIST_DELETED;
            return;
        }
        sites[nsites].ip = hist[i].ip;
        sites[nsites].hostname = hist[i].hostname;
        sites[nsites].n = malloc (sizeof (int)*MAX_HIST_PERSITE);
        sites[nsites].n[0] = i;
        sites[nsites].N = 1;
        nsites++;
    }
    // adding new member to existing group j
    else
    {
        if (sites[j].N == MAX_HIST_PERSITE)
        {
            hist[i].flags |= HIST_DELETED;
            return;
        }
        sites[j].n[sites[j].N] = i;
        sites[j].N++;
    }
}

/* -------------------------------------------------------------------
 */
int history_write (void)
{
    FILE   *hf;
    int    i;

    // open history file
    hf = try_open (options.history_file, "w");
    if (hf == NULL) return 1;

    for (i=0; i<nhist; i++)
    {
        if (hist[i].flags & HIST_DELETED) continue;
        history_write_line (hf, hist[i].tm, hist[i].hostname,
                            hist[i].ip, hist[i].userid,
                            hist[i].port, hist[i].flags, hist[i].directory);
        if (!options.slowlink && i != 0 && i % 1000 == 0)
            put_message (M("Writing history file;  %u lines done"), i);
    }

    fclose (hf);
    return 0;
}

/* -------------------------------------------------------------------
 */
static void history_free (void)
{
    int i;

    // free storage
    for (i=0; i<nsites; i++)
        free (sites[i].n);

    //for (i=0; i<nhist; i++)
    //{
    //    free (hist[i].hostname);
    //    free (hist[i].directory);
    //    if (hist[i].userid[0] != '\0') free (hist[i].userid);
    //}
    free (sites);
    free (hist);
    if (chunks != NULL) chunk_free (chunks);
    chunks = NULL;
}

/* -------------------------------------------------------------------
 */
int history_read_line (FILE *hf, time_t *timestamp, char *hostname, unsigned long *ip,
                       char *userid, unsigned int *port,
                       int *flags, char *directory)
{
    char     buffer[4096];
    char     *p, *p1, psw[64];

    do
    {
        if (fgets (buffer, sizeof(buffer), hf) == NULL) return 1;
    }
    while (str_numchars (buffer, ',') != 8);

    p = buffer;
    // time
    p1 = strchr (p, ',');
    *p1 = '\0';
    *timestamp = strtoul (p, NULL, 10);
    // hostname
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    str_copy (hostname, p, 64);
    // IP
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    *ip = inet_addr (p);
    // port
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    *port = atoi (p);
    if (*port == 0) *port = options.defaultport;
    // directory
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    str_copy (directory, p, 1024);
    // userid
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    str_copy (userid, p, 64);
    // password
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    if (strcmp (p, "*") != 0 && p[0] != '\0')
    {
        str_copy (psw, p, 64);
        p = unscramble (psw);
        if (options.psw_enctype == 0) options.psw_enctype = 2;
        psw_add (hostname, userid, p);
        free (p);
    }
    // flags
    p = p1 + 1;
    p1 = strchr (p, ',');
    *p1 = '\0';
    *flags = breakflags (p);

    //dmsg ("read: buffer: %s\n time=%u, hostname=%s, ip=%lu, userid=%s, password=%s,\n port=%u, flags=%x, dir=%s\n",
    //      buffer, *timestamp, hostname, *ip, userid, password, *port, *flags, directory);

    return 0;
}

/* -------------------------------------------------------------------
 */
void history_write_line (FILE *hf, time_t tm, char *hostname, unsigned long ip,
                         char *userid, int port, int flags, char *directory)
{
    struct in_addr     in_addr1;
    char               cport[64];

    in_addr1.s_addr = ip;

    // fix defaults
    if (port != options.defaultport)
        snprintf1 (cport, sizeof(cport), "%u", port);
    else
        strcpy (cport, "");

    fprintf (hf,
             "%u,%s,%s,%s,%s,%s,,%s,\n",
             (int)tm, hostname, inet_ntoa (in_addr1),
             cport, directory,
             strcmp (userid, options.anon_name) == 0 ? "" : userid,
             makeflags (flags));
}

/* -------------------------------------------------------------------
 returns 0 if nickname was not found, 1 otherwise
 */

#define MAX_CHOICES 8192

int nchoices, *choices;

int history_nickname (char *nickname, url_t *u)
{
    int     i, nch;

    if (history_read (FALSE) != 0)
    {
        fly_ask_ok (0, M("Error reading history file %s"), options.history_file);
        return 0;
    }

    if (nhist == 0)
    {
        fly_ask_ok (0, M("History file %s is empty"), options.history_file);
        return 0;
    }

    nchoices = 0;
    choices = malloc (MAX_CHOICES * sizeof (int));
    // look in hostnames
    for (i=0; i<nhist; i++)
        if (str_stristr (hist[i].hostname, nickname) != NULL)
            if (nchoices < MAX_CHOICES-1)
            {
                // check whether this site already included
                //for (j=0; j<nchoices; j++)
                //    if (same_site(i, choices[j])) break;
                //if (j == nchoices)
                choices[nchoices++] = i;
            }

    // look in pathnames
    for (i=0; i<nhist; i++)
        if (str_stristr (hist[i].directory, nickname) != NULL)
            if (nchoices < MAX_CHOICES-1)
            {
                // check whether this site already included
                //for (j=0; j<nchoices; j++)
                //    if (same_site(i, choices[j])) break;
                //if (j == nchoices)
                    choices[nchoices++] = i;
            }

    dmsg ("nchoices = %d\n", nchoices);
    for (i=0; i<nchoices; i++)
        dmsg ("%d) %s %s\n", i, hist[choices[i]].hostname,
              hist[choices[i]].directory);

    if (nchoices == 0)
        fly_ask_ok (ASK_WARN, M("Nickname '%s'\n"
                                "was not found in the history"), nickname);

    // found multiple choices?
    if (nchoices > 0)
        nch = fly_choose (M("Select history entry"), CHOOSE_ALLOW_HSCROLL,
                          &nchoices, 0, draw_hist_line, NULL);
    else
        nch = -1;

    // found anything?
    if (nch >= 0)
    {
        strcpy (u->hostname, hist[choices[nch]].hostname);
        u->port = hist[choices[nch]].port;
        u->ip = hist[choices[nch]].ip;
        strcpy (u->pathname, hist[choices[nch]].directory);
        strcpy (u->userid, hist[choices[nch]].userid);
        u->password[0] = '\0';
        u->flags = hist[choices[nch]].flags;
    }

    history_free ();
    free (choices);

    return (nch >= 0);
}

/* ------------------------------------------------------------- */

void draw_hist_line (int n, int len, int shift, int pointer, int row, int col)
{
    int  l, num;
    char buffer[1024];

    video_put_n_cell (' ', pointer ?
                      options.attr_bmrk_pointer : options.attr_bmrk_back,
                      len, row, col);

    num = choices[n];
    snprintf1 (buffer, sizeof(buffer), " %s:%s ", hist[num].hostname, hist[num].directory);
    l = strlen (buffer);
    if (shift < l)
        video_put_str_attr (buffer+shift, min1(len, l-shift), row, col,
                            pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back);
}

/* ------------------------------------------------------------------- */

int history_select (url_t *u)
{
    int     i, s, n;
    char    buf[1024];

    if (history_read (TRUE) != 0)
    {
        fly_ask_ok (0, M("Error reading history file %s"), options.history_file);
        return -1;
    }

    if (nhist == 0)
    {
        fly_ask_ok (0, M("History file %s is empty"), options.history_file);
        return -1;
    }

    // write history to file
    history_write ();

    // make a selection
    f_fline    = 0;
    view       = 2;
    selection  = -2;
    f_cursor   = 0;

    // try to set cursor
    if (u->hostname[0] != '\0')
    {
        for (i=0; i<nsites; i++)
            if (strcmp (sites[i].hostname, u->hostname) == 0) break;
        if (i != nsites) f_cursor = i;
    }

    changed = 0;
    do
    {
        if (view == 2)
        {
            snprintf1 (buf, sizeof(buf), M("History: %u sites, %u entries total"), nsites, nhist);
            s = fly_choose (buf, CHOOSE_RIGHT_IS_ENTER, &nsites, f_cursor,
                            drawline_site, callback_site);
            if (s >= 0)
            {
                f_cursor = s;
                view = 1;
            }
            if (s == -1)
            {
                selection = -1;
            }
        }
        else
        {
            snprintf1 (buf, sizeof(buf), M("History: %s, %u directories"), sites[f_cursor].hostname,
                     sites[f_cursor].N);
            s = fly_choose (buf, CHOOSE_LEFT_IS_ESC, &(sites[f_cursor].N), 0,
                            drawline_dir, callback_dir);
            if (s >= 0)
            {
                selection = s;
            }
            if (s == -1)
            {
                view = 2;
            }
        }
    }
    while (selection == -2);
    update (1);
    if (changed && fly_ask (ASK_YN, M("Save changes in history files?"), NULL))
        history_write ();

    // activate selected bookmark
    if (selection != -1)
    {
        init_url (u);
        n = sites[f_cursor].n[selection];
        strcpy (u->hostname, hist[n].hostname);
        u->port = hist[n].port;
        if (time(NULL) - hist[n].tm < 86400) u->ip = hist[n].ip;
        strcpy (u->pathname, hist[n].directory);
        strcpy (u->userid, hist[n].userid);
        u->password[0] = '\0';
        u->flags = hist[n].flags;
    }

    history_free ();

    if (selection == -1) return -1;
    return 0;
}

/* -------------------------------------------------------------------  */

void drawline_site (int n, int len, int shift, int pointer, int row, int col)
{
    struct in_addr  in_addr1;
    struct tm   *tmdt;
    char buf[1024];

    memset (buf, ' ', len);
    in_addr1.s_addr = sites[n].ip;
    tmdt = localtime (&(hist[sites[n].n[0]].tm));
    snprintf1 (buf, sizeof(buf), " %02u/%02u/%04u %02u:%02u  %s (%s) ",
             tmdt->tm_mday, tmdt->tm_mon+1, 1900+tmdt->tm_year,
             tmdt->tm_hour, tmdt->tm_min,
             sites[n].hostname, inet_ntoa (in_addr1));

    if (pointer)
        video_put_str_attr (buf, len, row, col, options.attr_bmrk_pointer);
    else
        video_put_str_attr (buf, len, row, col, options.attr_bmrk_back);
}

/* -------------------------------------------------------------------  */

void drawline_dir (int n, int len, int shift, int pointer, int row, int col)
{
    struct tm   *tmdt;
    char        buf[1024];
    int         ni;

    video_put_n_cell (' ', options.attr_bmrk_back, len, row, col);

    ni = sites[f_cursor].n[n];
    memset (buf, ' ', sizeof (buf));
    tmdt = localtime (&(hist[ni].tm));
    if (hist[ni].port != options.defaultport)
        snprintf1 (buf, sizeof(buf), "%02u/%02u/%04u %02u:%02u %s %s :%u%s",
                 tmdt->tm_mday, tmdt->tm_mon+1, 1900+tmdt->tm_year,
                 tmdt->tm_hour, tmdt->tm_min,
                 hist[ni].userid,
                 makeflags (hist[ni].flags),
                 hist[ni].port, hist[ni].directory);
    else
        snprintf1 (buf, sizeof(buf), "%02u/%02u/%04u %02u:%02u %s %s %s",
                 tmdt->tm_mday, tmdt->tm_mon+1, 1900+tmdt->tm_year,
                 tmdt->tm_hour, tmdt->tm_min,
                 hist[ni].userid,
                 makeflags (hist[ni].flags),
                 hist[ni].directory);

    video_put_str_attr (buf, len, row, col,
                        pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back);
}

/* -------------------------------------------------------------------  */

int  callback_site (int *nchoices, int *n, int key)
{
    static char *buffer = NULL;
    int         rc, i;

    if (IS_KEY (key))
    {
        switch (key)
        {
        case _F1:
            help (M_HLP_HISTORY);
            break;

        case _Delete:
            if (!fly_ask (ASK_YN|ASK_WARN,
                          M("%s\n"
                            "Delete site containing %u directory entries?"),
                          NULL,
                          sites[*n].hostname, sites[*n].N)) break;
            for (i=0; i<sites[*n].N; i++)
                hist[sites[*n].n[i]].flags |= HIST_DELETED;
            for (i=*n; i<nsites-1; i++)
                sites[i] = sites[i+1];
            nsites--;
            changed = 1;
            if (nsites == 0) return -1;
            break;

        case _CtrlF:
        ask_for_target:
            if (buffer != NULL) free (buffer);
            buffer = malloc (1024);
            buffer[0] = '\0';
            rc = entryfield (M("Look up forward"), buffer, 1024, 0);
            buffer = realloc (buffer, strlen (buffer)+1);
            if (rc != PRESSED_ENTER || buffer[0] == '\0') break;

        case _CtrlG:
            if (buffer == NULL || buffer[0] == '\0') goto ask_for_target;
            for (i=*n+1; i<nsites; i++)
            {
                if (str_stristr (sites[i].hostname, buffer) != NULL) break;
            }
            if (i == nsites)
                fly_ask (0, M("Cannot find '%s'"), NULL, buffer);
            else
                *n = i;
            break;

        case _CtrlH:
            if (buffer == NULL || buffer[0] == '\0')
            {
                if (buffer != NULL) free (buffer);
                buffer = malloc (1024);
                buffer[0] = '\0';
                rc = entryfield (M("Look up backwards"), buffer, 1024, 0);
                buffer = realloc (buffer, strlen (buffer)+1);
                if (rc != PRESSED_ENTER || buffer[0] == '\0') break;
            }
            for (i=*n-1; i>=0; i--)
            {
                if (str_stristr (sites[i].hostname, buffer) != NULL) break;
            }
            if (i < 0)
                fly_ask (0, M("Cannot find '%s'"), NULL, buffer);
            else
                *n = i;
            break;
        }
    }
    return -2;
}

/* -------------------------------------------------------------------  */

int callback_dir (int *nchoices, int *n, int key)
{
    static char  *buffer = NULL;
    int          i, j, rc;

    if (IS_KEY (key))
    {
        switch (key)
        {
        case _F1:
        case '?':
            help (M_HLP_HISTORY);
            break;

        case _Delete:
            if (!fly_ask (ASK_YN, M("%s:%s\n"
                                    "Delete directory entry?"),
                          NULL, sites[f_cursor].hostname,
                      hist[sites[f_cursor].n[*n]].directory)) break;
            hist[sites[f_cursor].n[*n]].flags |= HIST_DELETED;
            for (i=*n; i<sites[f_cursor].N-1; i++)
                sites[f_cursor].n[i] = sites[f_cursor].n[i+1];
            sites[f_cursor].N--;
            if (sites[f_cursor].N == 0)
            {
                for (i=f_cursor; i<nsites-1; i++)
                    sites[i] = sites[i+1];
                nsites--;
                view = 2;
            }
            changed = 1;
            if (nsites == 0) return -1;
            break;

        case _CtrlF:
        ask_for_target:
            if (buffer != NULL) free (buffer);
            buffer = malloc (1024);
            buffer[0] = '\0';
            rc = entryfield (M("Look up forward"), buffer, 1024, 0);
            buffer = realloc (buffer, strlen (buffer)+1);
            if (rc != PRESSED_ENTER || buffer[0] == '\0') break;

        case _CtrlG:
            if (buffer == NULL || buffer[0] == '\0') goto ask_for_target;
            // first we search current site
            for (i=*n+1; i<sites[f_cursor].N-1; i++)
            {
                if (str_stristr (hist[sites[f_cursor].n[i]].directory, buffer) != NULL) break;
            }
            if (i < sites[f_cursor].N-1)
            {
                *n = i;
            }
            else
            {
                if (!fly_ask (ASK_YN, M("Cannot find at %s. Search other sites?"),
                              NULL, sites[f_cursor].hostname)) break;
                for (i=f_cursor+1; i<nsites; i++)
                {
                    for (j=0; j<sites[i].N-1; j++)
                    {
                        if (str_stristr (hist[sites[i].n[j]].directory, buffer) != NULL) break;
                    }
                    if (j != sites[i].N-1) break;
                }
                if (i != nsites)
                {
                    f_cursor = i;
                    return -1;
                }
                fly_ask (0, M("Cannot find '%s'"), NULL, buffer);
            }
            break;

        case _CtrlH:
            if (buffer == NULL || buffer[0] == '\0')
            {
                if (buffer != NULL) free (buffer);
                buffer = malloc (1024);
                buffer[0] = '\0';
                rc = entryfield (M("Look up forward"), buffer, 1024, 0);
                buffer = realloc (buffer, strlen (buffer)+1);
                if (rc != PRESSED_ENTER || buffer[0] == '\0') break;
            }
            // first we search current site
            for (i=*n-1; i>=0; i++)
            {
                if (str_stristr (hist[sites[f_cursor].n[i]].directory, buffer) != NULL) break;
            }
            if (i >= 0)
            {
                *n = i;
            }
            else
            {
                if (!fly_ask (ASK_YN, M("Cannot find at %s. Search other sites?"), NULL, sites[f_cursor].hostname)) break;
                for (i=f_cursor-1; i>=0; i--)
                {
                    for (j=0; j<sites[i].N-1; j++)
                    {
                        if (str_stristr (hist[sites[i].n[j]].directory, buffer) != NULL) break;
                    }
                    if (j != sites[i].N-1) break;
                }
                if (i != -1)
                {
                    f_cursor = i;
                    return -1;
                }
                fly_ask (0, M("Cannot find '%s'"), NULL, buffer);
            }
            break;
        }
    }
    return -2;
}

/* -------------------------------------------------------------------
 returns time since first invocation of NFTP, in days
 */
int history_usage (void)
{
    int     i, rc;
    time_t  t = time (NULL);

    rc = history_read (FALSE);
    if (rc) return 0;

    for (i=0; i<nhist; i++)
    {
        t = min1 (t, hist[i].tm);
    }

    history_free ();

    return max1 ((time (NULL) - t)/(24*60*60), 0);
}

/* -------------------------------------------------------------------
 compares two history entries for sort
 */
static int hist_compare (__const__ void *elem1, __const__ void *elem2)
{
    struct entry_t *h1 = (struct entry_t *)elem1;
    struct entry_t *h2 = (struct entry_t *)elem2;

    switch (hist_sort_order)
    {
    case ORDER_HOSTNAME:
        //return strcmp (h1->hostname, h2->hostname);
        //return strcmp (h1->directory, h2->directory);
        return h1->hash - h2->hash;

    default:
    case ORDER_TIME:
        return h1->tm - h2->tm;
    }
}

