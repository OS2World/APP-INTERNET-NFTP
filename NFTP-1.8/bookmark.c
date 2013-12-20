#include <fly/fly.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "nftp.h"
#include "auxprogs.h"
#include "passwords.h"

// current limits

#define MAX_BM_PER_FOLDER  1024
#define FOLDER_NAME_LEN    128

struct folder
{
    char             *title;
    int              n_elems;
    int              bm[MAX_BM_PER_FOLDER];
};

#define bmk(i) (((url_t *)marks)[i])
#define fld(i) (((struct folder *)folders)[i])

static char *marks = NULL, *folders = NULL;
static int  nb, nf, nba, nfa;

#define VIEW_FOLDERS        1
#define VIEW_INSIDE_FOLDER  2

// changed: 1 if bookmark list has changed and should be saved.
static int   changed;

// _cursor: cursor for folders. counted from 0
//    f_cursor is index in "folders" array
// m_cursor: cursor for bookmarks. counted from 0
//    m_cursor is index in "folder[i].bm" array (!)
static int   f_cursor = 0, m_cursor = 0;

static int  read_bookmarks (void);
static int  write_bookmarks (void);
static int  insert_bookmark (url_t *u);
static char *makelpf (char *login, int flags);
static char *makeurl (char *site, char *initdir, int port);

int parse_bmk_line (char *part1, char *part2, url_t *u);
void draw_bmk_line (int n, int len, int shift, int pointer, int row, int col);

void drawline_folders (int n, int len, int shift, int pointer, int row, int col);
void drawline_entries (int n, int len, int shift, int pointer, int row, int col);
int  callback_folders (int *nchoices, int *n, int key);
int  callback_entries (int *nchoices, int *n, int key);

/* -------------------------------------------------------------
"read_bookmarks" fills "marks, folders, nb, nf"
return 0 if OK, 1 if unable to find or read the file
parsing errors are ignored but reported. this is nonfatal error */
    
static int read_bookmarks (void)
{
    FILE    *fd;
    char    buf[1024], buf1[1024], bmfile[MAX_PATH], *p;
    char    *part1, *part2, *part3;

    // sanity check
    if (marks != NULL) fly_error ("attempt to read bookmarks without previous freeing of marks");
    if (folders != NULL) fly_error ("attempt to read bookmarks without previous freeing of folders");

    // allocate memory. write_bookmarks frees it
    nba = 512;
    nfa = 64;
    marks    = malloc (nba*sizeof(url_t));
    folders  = malloc (nfa*sizeof(struct folder));
    nb = 0;
    nf = 0;

    // try to find nftp.bm file if bmk file is missing
    if (access (options.bmk_name, F_OK) != 0)
    {
        strcpy (bmfile, paths.system_libpath);
        str_cats (bmfile, "nftp.bm");
        if (access (bmfile, R_OK) == 0)
            copy_file (bmfile, options.bmk_name);
        
        strcpy (bmfile, paths.user_libpath);
        str_cats (bmfile, "nftp.bm");
        if (access (bmfile, R_OK) == 0)
            copy_file (bmfile, options.bmk_name);
    }
    
    // check whether bookmark file is present
    if (access (options.bmk_name, R_OK) != 0)
    {
        fly_ask_ok (0, M("No bookmarks found"));
        goto finishing;
    }

    // ...and try to open it
    fd = try_open (options.bmk_name, "r");
    if (fd == NULL)
    {
        fly_ask_ok (ASK_WARN, M("Cannot open bookmarks file [%s] for reading"), options.bmk_name);
        return 1;
    }
    
    // build a list of available bookmarks
    while (1)
    {
        if (fgets (buf, sizeof (buf), fd) == NULL) break;
        
        // remove trailing newline
        str_strip (buf, "\r\n ");
        
        // skip empty lines and comments
        if (strlen (buf) <= 1 || buf[0] == ';') continue;
        
        // make a copy for error reporting
        strcpy (buf1, buf); 

        // check for folder header
        if (buf[0] == '[')
        {
            p = strrchr (buf, ']');
            if (p != NULL) *p = '\0';
            fld(nf).title = strdup (buf+1);
            fld(nf).n_elems = 0;
            if (nf == nfa-1)
            {
                nfa *= 2;
                folders = realloc (folders, nfa*sizeof(struct folder));
            }
            nf++;
            continue;
        }

        if (nf == 0) continue; // ignore entries outside of folders
        
        // find main separators (" : ")
        part1 = buf;
        while (*part1 == ' ') part1++; // ignore starting spaces
        
        p = strstr (part1, " : ");
        if (p == NULL) goto corrupted;   // corrupted entry
        part2 = p + 3;
        while (*part2 == ' ') part2++; // ignore starting spaces
        while (*p == ' ') *(p--) = '\0'; // discard trailing spaces
        
        p = strstr (part2, " :");
        if (p == NULL) goto corrupted;   // corrupted entry
        part3 = p + 2;
        while (*part3 == ' ') part3++; // ignore starting spaces
        while (*p == ' ') *(p--) = '\0'; // discard trailing spaces

        init_url (&bmk(nb));
        if (parse_bmk_line (part1, part2, &bmk(nb))) goto corrupted;
        if (bmk(nb).password[0] != '\0')
        {
            if (options.psw_enctype == 0) options.psw_enctype = 2;
            psw_add (bmk(nb).hostname, bmk(nb).userid, bmk(nb).password);
            bmk(nb).password[0] = '\0';
        }
        
        /* ----------------------------------------------------- */
        
        // description
        str_scopy (bmk(nb).description, part3);

        // here we finally have a new entry...
        fld(nf-1).bm[fld(nf-1).n_elems] = nb;
        fld(nf-1).n_elems++;
        if (nb == nba-1)
        {
            nba *= 2;
            marks = realloc (marks, nba*sizeof(url_t));
        }
        nb++;
        continue;
        
    corrupted:
        fly_ask_ok (0, M("Bookmark entry\n"
                         "%s\n"
                         "is corrupted and will be ignored"), buf1);
    }
    
    fclose (fd);

    // no entries found??
    if (nb == 0)
        fly_ask_ok (0, M("Bookmark list is empty"));

finishing:
    
    // if there are no folders, create one
    if (nf == 0)
    {
        fld(0).title = strdup (M("General sites (created by default)"));
        fld(0).n_elems = 0;
        nf++;
    }

    // check selection bar position
    changed = FALSE;
    if (f_cursor >= nf) f_cursor = 0;
    if (m_cursor >= fld(f_cursor).n_elems) m_cursor = 0;

    return 0;
}

/* -------------------------------------------------------------
 "write_bookmarks" saves bookmark list into the text file
 returns 0 if OK, 1 if error occured
 */

static int write_bookmarks (void)
{
    FILE   *fd;
    int    i, j;

    if (!changed) return 0;
    if (fly_ask (ASK_YN, M("Bookmark information has been changed.\n"
                           "Save new bookmark list?"), NULL) == 0) return 0;
    
    // open bookmark file for writing
    fd = try_open (options.bmk_name, "w");
    if (fd == NULL)
    {
        fly_ask_ok (ASK_WARN, M("Cannot open bookmarks file [%s] for writing"), options.bmk_name);
        return 1;
    }
    
    // saving entries. format is
    // ftp.cdrom.com:21/pub/os2 : asv/Sethanon/flags : description
    fprintf (fd, M(
"; bookmark format is\n"
"; ftp.sai.msu.su:21/pub/os2 : login/password/flags : description\n"
";\n"
"; edit this file with care; following rules apply:\n"
"; - spaces around colons ARE important;\n"
"; - pathnames must be absolute, i.e. start with '/';\n"
"; - omit 'port' if you want to;\n"
"; - substitute '*' for anonymous 'login' and 'password':\n"
";   crydee.sai.msu.su:/pub/comp/os/os2 : */*/flags : SAI OS/2 archive\n"
";\n"
";\n"));
    for (i=0; i<nf; i++)
    {
        fprintf (fd, "\n[%s]\n", fld(i).title);
        for (j=0; j<fld(i).n_elems; j++)
        {
            fprintf (fd, "%s : %s : %s\n",
                     makeurl (bmk(fld(i).bm[j]).hostname,
                              bmk(fld(i).bm[j]).pathname,
                              bmk(fld(i).bm[j]).port),
                     makelpf (bmk(fld(i).bm[j]).userid,
                              bmk(fld(i).bm[j]).flags),
                     bmk(fld(i).bm[j]).description);
        }
    }

    // let's assume no errors happened during write
    fclose (fd);
    
    return 0;
}

/* ------------------------------------------------------------- */

void bookmark_add (url_t *u)
{
    int i;
    
    // read bookmarks into memory
    if (read_bookmarks ()) return;

    // check out whether this bookmark is already present
    for (i=0; i<nb; i++)
        if (strcmp (bmk(i).hostname, u->hostname) == 0)
        {
            if (options.save_marks || 
                !fly_ask (ASK_YN, M("The site '%s' is already bookmarked\n"
                                    "(directory '%s'). Add it anyway?"), NULL,
                      bmk(i).hostname, bmk(i).pathname))
                goto no_change;
        }

    if (u->description[0] != '\0' ||
        entryfield (M("Enter description:"), u->description,
                        sizeof(u->description), 0) != PRESSED_ESC)
        insert_bookmark (u);
    
    // check for editing bookmarks
    write_bookmarks ();

no_change:
    for (i=0; i<nf; i++) free (fld(i).title);
    free (marks);
    free (folders);
    marks    = NULL;
    folders  = NULL;
    nb   = 0;
    nf = 0;
}

/* -------------------------------------------------------------
 returns 0 if nickname was not found, 1 otherwise
 */

#define MAX_CHOICES 256

int bookmark_nickname (char *nickname, url_t *u)
{
    int     i, j, nchoices, nch, choices[MAX_CHOICES];
    
    // read bookmarks into memory
    if (read_bookmarks ()) return 0;

    nchoices = 0;
    // look for nick in hostname
    for (i=0; i<nb; i++)
        if (str_stristr (bmk(i).hostname, nickname) != NULL)
            if (nchoices < MAX_CHOICES-1)
                choices[nchoices++] = i;

    // look for nick in description, skip already marked entries
    for (i=0; i<nb; i++)
        if (str_stristr (bmk(i).description, nickname) != NULL)
            if (nchoices < MAX_CHOICES-1)
            {
                for (j=0; j<nchoices; j++)
                    if (i == choices[j]) break;
                if (j == nchoices)
                    choices[nchoices++] = i;
            }
    
    // look for nick in pathname, skip already marked entries
    for (i=0; i<nb; i++)
        if (str_stristr (bmk(i).pathname, nickname) != NULL)
            if (nchoices < MAX_CHOICES-1)
            {
                for (j=0; j<nchoices; j++)
                    if (i == choices[j]) break;
                if (j == nchoices)
                    choices[nchoices++] = i;
            }

    //dmsg ("nchoices = %d\n", nchoices);
    //for (i=0; i<nchoices; i++)
    //    dmsg ("%d) %s %s\n", i, bmk(choices[i]).hostname,
    //          bmk(choices[i]).pathname);

    if (nchoices == 0)
    {
        fly_ask_ok (ASK_WARN, M("Nickname '%s'\n"
                                "was not found among the bookmarks"), nickname);
    }
    
    // found multiple choices?
    if (nchoices > 0)
        nch = fly_choose (M("Select bookmark"), 0, &nchoices, 0,
                          draw_bmk_line, NULL);
    else
        nch = -1;
    
    // found anything?
    if (nch >= 0) *u = bmk(choices[nch]);
    
    for (i=0; i<nf; i++) free (fld(i).title);
    free (marks);
    free (folders);
    marks    = NULL;
    folders  = NULL;
    nb   = 0;
    nf = 0;
    
    return (nch >= 0);
}

/* ------------------------------------------------------------- */

void draw_bmk_line (int n, int len, int shift, int pointer, int row, int col)
{
    char  displayed[2048], buf[2048], *p;
    int   url_pos, url_len;

    // clear buffer first
    memset (displayed, ' ', len);

    // form an URL
    if (bmk(n).userid[0] == '\0')
    {
        if (bmk(n).port == 0 || bmk(n).port == options.defaultport)
            snprintf1 (buf, sizeof(buf), " %s - %s",
                     bmk(n).hostname,
                     bmk(n).pathname);
        else
            snprintf1 (buf, sizeof(buf), " %s:%d - %s",
                     bmk(n).hostname,
                     bmk(n).port,
                     bmk(n).pathname);
    }
    else
    {
        if (bmk(n).port == 0 || bmk(n).port == options.defaultport)
            snprintf1 (buf, sizeof(buf), " %s@%s - %s",
                     bmk(n).userid,
                     bmk(n).hostname,
                     bmk(n).pathname);
        else
            snprintf1 (buf, sizeof(buf), " %s@%s:%d - %s",
                     bmk(n).userid,
                     bmk(n).hostname,
                     bmk(n).port,
                     bmk(n).pathname);
    }
    // `buf' now holds host/path

    // now form complete line and compute position or length of URL part
    p = bmk(n).description;
    while (*p != '\0' && *p == ' ') p++;
    if (*p == '\0')
    {
        // no description
        strcpy (displayed, buf);
        url_pos = 0;
        url_len = strlen (buf);
    }
    else
    {
        if (options.show_serverinfo)
        {
            snprintf1 (displayed, sizeof(displayed), " %s   [%s]",
                     bmk(n).description, buf);
            url_pos = strlen (bmk(n).description) + 5;
            url_len = strlen (buf);
        }
        else
        {
            strcpy (displayed, bmk(n).description);
            url_pos = 0;
            url_len = 0;
        }
    }
    url_len = max1 (0, min1 (url_len, len-url_pos));

    // display the line itself
    video_put_str_attr (displayed, len, row, col, pointer ?
                        options.attr_bmrk_pointer : options.attr_bmrk_back);
    
    if (url_len == 0) return;

    // draw URL attribute if necessary
    video_put_n_attr (pointer ? options.attr_bmrk_hostpath_pointer : options.attr_bmrk_hostpath,
                      url_len, row, col+url_pos);
}

/* ------------------------------------------------------------- */

int bookmark_select (url_t *u)
{
    static int     s, selection = -2;
    char           buf[1024];
    int            i;
    
    // read bookmarks into memory
    if (read_bookmarks ()) return 0;

    // make a selection
    //selection = -2;
    do
    {
        if (selection == -2)
        {
            s = fly_choose (M("Current bookmarks"), CHOOSE_RIGHT_IS_ENTER, &nf,
                        f_cursor, drawline_folders, callback_folders);
            if (s == -1) break;
            f_cursor = s;
        }

        snprintf1 (buf, sizeof(buf), M("Folder: %s"), fld(f_cursor).title);
        s = fly_choose (buf, CHOOSE_LEFT_IS_ESC, &fld(f_cursor).n_elems,
                        m_cursor, drawline_entries, callback_entries);
        if (s == -1)
        {
            selection = -2;
            m_cursor = 0;
        }
        if (s >= 0)
        {
            selection = s;
            m_cursor = s;
        }
    }
    while (selection == -2);
    update (1);

    // activate selected bookmark
    if (selection >= 0) *u = bmk (fld(f_cursor).bm[selection]);
    
    // check for editing bookmarks
    write_bookmarks ();

    for (i=0; i<nf; i++) free (fld(i).title);
    free (marks);
    free (folders);
    marks    = NULL;
    folders  = NULL;
    nb   = 0;
    nf = 0;
    
    if (selection < 0) return 0;
    
    return 1;
}

/* ------------------------------------------------------------- */

void drawline_folders (int n, int len, int shift, int pointer, int row, int col)
{
    char    buf[1024];
    
    memset (buf, ' ', sizeof (buf));
    snprintf1 (buf, sizeof(buf), " -> %s", fld(n).title);
    
    video_put_str_attr (buf, len, row, col,
                        pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back);
}

/* ------------------------------------------------------------- */

void drawline_entries (int n, int len, int shift, int pointer, int row, int col)
{
    draw_bmk_line (fld(f_cursor).bm[n], len, shift, pointer, row, col);
}

/* ------------------------------------------------------------- */

int callback_folders (int *nchoices, int *n, int key)
{
    int    i;
    char   buf[FOLDER_NAME_LEN];
    struct folder fl;

    if (IS_KEY (key))
    {
        switch (key)
        {
        case _F1:
            help (M_HLP_BOOKMARK1);
            break;

        case _CtrlUp:
        case 'U':
            if (*n == 0) break;
            fl = fld(*n);
            fld(*n) = fld(*n-1);
            fld(*n-1) = fl;
            changed = TRUE;
            (*n)--;
            break;

        case _CtrlDown:
        case 'D':
            if (*n == nf-1) break;
            fl = fld(*n);
            fld(*n) = fld(*n+1);
            fld(*n+1) = fl;
            changed = TRUE;
            (*n)++;
            break;

        case _Insert:
            if (nf >= nfa-1)
            {
                nfa *= 2;
                folders = realloc (folders, nfa*sizeof(struct folder));
            }
            buf[0] = '\0';
            if (entryfield (M("Enter name for new folder"), buf, sizeof (buf), 0) == PRESSED_ESC) break;
            if (buf[0] == '\0') break;
            fld(nf).title = strdup (buf);
            fld(nf).n_elems = 0;
            nf++;
            changed = TRUE;
            break;

        case _Delete:
            if (fld(*n).n_elems != 0)
                if (fly_ask (ASK_YN, M("'%s'\n"
                                       "Delete non-empty folder?"), NULL,
                             fld(*n).title) == FALSE)
                    break;
            for (i=*n; i<nf-1; i++)
                fld(i) = fld(i+1);
            nf--;
            changed = TRUE;
            *n = min1 (nf-1, *n);
            break;

        case _CtrlE:
            strcpy (buf, fld(*n).title);
            if (entryfield (M("Enter folder title:"), buf, sizeof(buf), 0) == PRESSED_ESC) break;
            strcpy (fld(*n).title, buf);
            changed = TRUE;
            break;
        }
    }
    return -2;
}

/* ------------------------------------------------------------- */

int callback_entries (int *nchoices, int *n, int key)
{
    int    i, rc, bk;
    url_t    u;
    
    if (IS_KEY(key))
    {
        switch (key)
        {
        case _F1:
        case '?':
            help (M_HLP_BOOKMARK2);
            break;

        case _CtrlUp:
        case 'U':
            if (fld(f_cursor).n_elems < 1) break;
            if (*n == 0) break;
            bk = fld(f_cursor).bm[*n];
            fld(f_cursor).bm[*n] = fld(f_cursor).bm[*n-1];
            fld(f_cursor).bm[*n-1] = bk;
            changed = TRUE;
            (*n)--;
            break;

        case _CtrlDown:
        case 'D':
            if (fld(f_cursor).n_elems < 1) break;
            if (*n == fld(f_cursor).n_elems-1) break;
            bk = fld(f_cursor).bm[*n];
            fld(f_cursor).bm[*n] = fld(f_cursor).bm[*n+1];
            fld(f_cursor).bm[*n+1] = bk;
            changed = TRUE;
            (*n)++;
            break;

        case _CtrlLeft:
        case '<':
            if (fld(f_cursor).n_elems < 1) break;
            if (!insert_bookmark (& bmk(fld(f_cursor).bm[*n]))) break;
            for (i=*n; i<fld(f_cursor).n_elems-1; i++)
                fld(f_cursor).bm[i] = fld(f_cursor).bm[i+1];
            fld(f_cursor).n_elems--;
            changed = TRUE;
            *n  = max1 (0, min1 (fld(f_cursor).n_elems-1, *n));
            break;

        case _Insert:
            init_url (&u);
            rc = Edit_Site (&u, FALSE);
            if (rc == PRESSED_ESC) break;
            fld(f_cursor).bm[fld(f_cursor).n_elems] = nb;
            bmk(nb) = u;
            nb++;
            fld(f_cursor).n_elems++;
            changed = TRUE;
            break;

        case _Delete:
            if (fld(f_cursor).n_elems < 1) break;
            //if (fly_ask (ASK_YN, M("Delete bookmark?"), NULL) == FALSE) break;
            if (fly_ask (ASK_YN, "delete?", NULL) == FALSE) break;
            for (i=*n; i<fld(f_cursor).n_elems-1; i++)
                fld(f_cursor).bm[i] = fld(f_cursor).bm[i+1];
            fld(f_cursor).n_elems--;
            changed = TRUE;
            *n  = max1 (0, min1 (fld(f_cursor).n_elems-1, *n));
            break;

        case _CtrlE:
            u = bmk(fld(f_cursor).bm[*n]);
            rc = Edit_Site (&u, FALSE);
            if (rc == PRESSED_ESC) break;
            bmk(fld(f_cursor).bm[*n]) = u;
            changed = TRUE;
            break;
        }
    }
    return -2;
}

/* -------------------------------------------------------------
returns 0 if cancelled
        1 if successful */

static int insert_bookmark (url_t *u)
{
    int  selection;

    selection = fly_choose (M("Current bookmarks"), 0, &nf,
                            f_cursor, drawline_folders, callback_folders);
    
    // put bookmark into selected folder
    if (selection != -1 && fld(selection).n_elems != MAX_BM_PER_FOLDER-1)
    {
        fld(selection).bm[fld(selection).n_elems] = nb;
        nb++;
        bmk(fld(selection).bm[fld(selection).n_elems]) = *u;
        fld(selection).n_elems++;
        changed = TRUE;
    }

    if (selection == -1) return 0;
    else                 return 1;
}

/* ------------------------------------------------------------- */

static char *makeurl (char *site, char *initdir, int port)
{
    static char buffer[1024];
    
    if (port != 0 && port != options.defaultport)
        snprintf1 (buffer, sizeof(buffer), "%s:%d%s%s",
                 site, port,
                 initdir[0] == '/' ? "" : ":",
                 initdir);
    else
        snprintf1 (buffer, sizeof(buffer), "%s:%s",
                 site, initdir);
    return buffer;
}

/* ------------------------------------------------------------- */

static char *makelpf (char *login, int flags)
{
    static char buffer[1024];

    snprintf1 (buffer, sizeof(buffer), "%s//%s",
             login[0] == '\0' ? "*" : login,
             makeflags (flags));
    
    return buffer;
}

/* -------------------------------------------------------------
 parse_bmk_line breaks bookmark line into parts.
 Returns 0 if success, 1 if error
 */
int parse_bmk_line (char *url, char *lpf, url_t *u)
{
    char   *p, *p1, *p2, *ps;
    int    rc;

    // process URL

    //dmsg ("parsing [%s][%s]\n", url, lpf);
    rc = 0;
    p = strdup (url);
    rc = parse_url (p, u);
    free (p);
    if (rc) return rc;

    // process LPF (login, password, flags)

    u->flags = 0;
    p  = strdup (lpf);
    p1 = strstr (p, "//");
    if (p1 != NULL)
    {
        // no password for sure
        *p1 = '\0';
        u->flags = breakflags (p1+2);
    }
    else
    {
        p1 = strchr (p, '/'); // points to / before password
        p2 = strrchr (p, '/'); // points to / before flags
        if (p1 != NULL)
        {
            *p1 = '\0';
            if (p2 != p1)
            {
                *p2 = '\0';
                u->flags = breakflags (p2+1);
            }
            ps = unscramble (p1+1);
            str_scopy (u->password, ps);
            free (ps);
        }
    }
    str_scopy (u->userid, p);
    free (p);

    if (strcmp (u->userid, "*")   == 0) u->userid[0] = '\0';
    if (strcmp (u->password, "*") == 0) u->password[0] = '\0';
    
    return 0;
}
