#include <fly/fly.h>

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <manycode.h>
#include "nftp.h"
#include "local.h"
#include "net.h"

#include "auxprogs.h"

#ifdef __MINGW32__
#include <process.h>
#endif

static void compose_buffer (entryfield_t *t, char *buffer, int hide);
static char *yes1 = NULL, *no1 = NULL;

/* ------------------------------------------------------------- */

int Edit_Site (url_t *u, int allow_psw)
{
    int        reply, n;
    entryfield_t sitebuf[7];
    
    sitebuf[0].banner = MSG (M_SITE_EDIT1);
    sitebuf[0].type = EF_STRING;
    str_scopy (sitebuf[0].value.string, u->hostname);
    sitebuf[0].flags = 0;
        
    sitebuf[1].banner = MSG (M_SITE_EDIT5);
    sitebuf[1].type = EF_STRING;
    str_scopy (sitebuf[1].value.string, u->pathname);
    sitebuf[1].flags = 0;

    sitebuf[2].banner = MSG (M_SITE_EDIT2);
    sitebuf[2].type = EF_STRING;
    str_scopy (sitebuf[2].value.string, u->userid);
    sitebuf[2].flags = 0;

    if (allow_psw)
    {
        n = 0;
        sitebuf[3].banner = MSG (M_SITE_EDIT3);
        sitebuf[3].type = EF_STRING;
        str_scopy (sitebuf[3].value.string, u->password);
        sitebuf[3].flags = LI_INVISIBLE;
    }
    else
        n = 1;
        
    sitebuf[4-n].banner = MSG (M_SITE_EDIT4);
    sitebuf[4-n].type = EF_INTEGER;
    sitebuf[4-n].value.integer = u->port;
    sitebuf[4-n].flags = 0;

    sitebuf[5-n].banner = MSG (M_SITE_EDIT6);
    sitebuf[5-n].type = EF_STRING;
    str_scopy (sitebuf[5-n].value.string, makeflags (u->flags));
    sitebuf[5-n].flags = 0;

    sitebuf[6-n].banner = MSG(M_EDIT3);
    sitebuf[6-n].type = EF_STRING;
    str_scopy (sitebuf[6-n].value.string, u->description);
    sitebuf[6-n].flags = 0;

    reply = mle2 (7-n, sitebuf, MSG(M_USETAB2), M_HLP_SITE_EDIT);

    if (reply == PRESSED_ENTER)
    {
        // put values back
        if (strcmp (u->hostname, sitebuf[0].value.string) != 0) u->ip = 0L;
        str_scopy (u->hostname, sitebuf[0].value.string);
        str_scopy (u->pathname, sitebuf[1].value.string);
        str_scopy (u->userid, sitebuf[2].value.string);
        if (allow_psw)
            str_scopy (u->password, sitebuf[3].value.string);
        u->port = sitebuf[4-n].value.integer;
        u->flags = breakflags (sitebuf[5-n].value.string);
        str_scopy (u->description, sitebuf[6-n].value.string);
    }

    return reply;
}

/* ------------------------------------------------------------- */

void dmsg (char *format, ...)
{
    va_list       args;
    
    if (!options.debug) return;
    if (dbfile == NULL) return;

    va_start (args, format);
    fprintf (dbfile, "%12.2f: ", clock1());
    vfprintf (dbfile, format, args);
    fflush (dbfile);
    va_end (args);
}

/* ------------------------------------------------------------- */

static struct
{
    char *name;
    int  codepage;
}
charsets[] =
{
    {" Unknown ", EN_UNKNOWN},
    {" Alternative (DOS, cp866) ", EN_ALT},
    {" KOI8-R (Unix, cp878) ", EN_KOI8R},
    {" Macintosh ", EN_MAC},
    {" ISO8859-5 (Unix) ", EN_ISO},
    {" Windows-1251 (Windows) ", EN_WINDOWS}
};

void drawline_charsets (int n, int len, int shift, int pointer, int row, int col)
{
    int attr;

    attr = pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back;
    video_put_n_cell (' ', attr, len, row, col);
    len = min1 (len, strlen(charsets[n].name));
    video_put_str (charsets[n].name, len, row, col);
}

/* ------------------------------------------------------------- */

int select_charset (int enc)
{
    int  i, ncharsets, startpos, rc;

    ncharsets = sizeof(charsets) / sizeof(charsets[0]);
    startpos = 0;
    for (i=0; i<ncharsets; i++)
        if (charsets[i].codepage == enc) startpos = i;

    rc = fly_choose ("Source charset", 0, &ncharsets, startpos,
                     drawline_charsets, NULL);

    if (rc >= 0 && rc < ncharsets) return charsets[rc].codepage;

    return enc;
}
/* ------------------------------------------------------------- */

static char * refine_line (char *);

void ViewArray (char *s[], int nl, char *hdr)
{
    char buf[256], searchbuf[128], *p, *savbuf;
    int  i, fl, cl, ll, pg, fp, mp, spg, key, line, run=TRUE;
    int  c1, c2, r1, r2, enc;
    FILE *fh;

    // save screen
    savbuf = video_save (0, 0, video_vsize()-1, video_hsize()-1);
    
    fl = 0;
    fp = 0;
    mp = 256;
    spg = 10;
    searchbuf[0] = '\0';

    enc = EN_UNKNOWN;
    if (options.guess_cyrillic)
    {
        for (i=0; i<min1(nl, 10); i++)
        {
            if (is_8bit (s[i]) && strlen (s[i]) < 512)
            {
                enc = rcd_guess (s[i]);
                if (enc != EN_UNKNOWN) break;
            }
        }
    }

    do
    {
        // set up initial values
        c1 = 0;        c2 = video_hsize()-1;
        r1 = 1;        r2 = video_vsize()-1;
        pg = r2-r1;
        ll = max1 (0, nl+1-pg);
        
        // prepare screen
        video_clear (0, 0, video_vsize()-1, video_hsize()-1, fl_clr.viewer_text);
        video_put_n_attr (fl_clr.viewer_header, c2-c1+1, 0, c1);
        video_put_str (hdr, min1 (strlen (hdr), 50), 0, 0);
        
        // redraw screen
        memset (buf, ' ', sizeof (buf));
        snprintf1 (buf, sizeof(buf), " %c %d/%d %+d ", fl_sym.v, min1 (fl+pg, nl), nl, fp);
        video_put_str (buf, video_hsize()-50, 0, 50);
        for (line=r1; line <= r2; line++)
        {
            cl = line-r1 + fl;
            memset (buf, ' ', sizeof (buf));
            if (cl < nl)
            {
                p = refine_line (s[cl]);
                if (fp < strlen (p)) strncpy (buf, p+fp, c2-c1+1);
            }
            buf[c2-c1+1] = '\0';
            p = strstr (buf, searchbuf);
            if (options.guess_cyrillic)
                rcd_translate (buf, enc, fl_opt.charset);
            video_put_str_attr (buf, c2-c1+1, line, c1, fl_clr.viewer_text);
            if (p) video_put_n_attr (fl_clr.viewer_found, strlen(searchbuf),
                                     line, c1+p-buf);
        }
        video_update (0);

        key = getmessage (-1);
        if (IS_KEY(key))
        {
            switch (key)
            {
            case _F1:
            case '?':
                help (M_HLP_VIEWER);
            case _Up:
                if (fl) fl--; break;
            case _Down:
                if (fl < ll) fl++; break;
            case _Home:
                fl = 0; fp = 0; break;
            case _End:
                fl = ll; break;
            case _PgUp:
                fl = max1 (0, fl-pg); break;
            case _PgDn:
                fl = min1 (ll, fl+pg); break;
            case _Left:
                if (fp) fp--; break;
            case _CtrlLeft:
            case '<':
                fp = max1 (0, fp-spg); break;
            case _CtrlRight:
            case '>':
                fp = min1 (256, fp+spg); break;
            case _Right:
                if (fp < mp) fp++; break;
            case _F10:
            case _Esc:
                run = FALSE; break;
            case _F7:
            case _CtrlF:
                if (entryfield (MSG(M_ENTER_SEARCHSTRING), searchbuf, sizeof(searchbuf), 0) == PRESSED_ESC)
                    break;
            case _ShiftF7:
            case _CtrlG:
                for (i=fl+1; i<nl; i++)
                    if (strstr (s[i], searchbuf))
                    {
                        fl = i;
                        break;
                    }
                if (fl > ll) fl = ll;
                if (fl < 0) fl = 0;
                break;
            case _F2:
            case _CtrlW:
                buf[0] = '\0';
                if (entryfield (MSG(M_ENTER_FILENAME_SAVE), buf, sizeof(buf), 0) == PRESSED_ESC)
                    break;
                fh = fopen (buf, "w");
                if (fh == NULL)
                {
                    fly_ask_ok (ASK_WARN, MSG(M_CANNOT_OPEN), buf);
                    break;
                }
                for (i=0; i<nl; i++)
                {
                    fputs (s[i], fh);
                fputs ("\n", fh);
                }
                fclose (fh);
                fly_ask_ok (0, MSG(M_FILE_SAVED));
                break;
            case _F8:
                enc = select_charset (enc);
                break;
            default:
                break;
            }
        }
    }
    while (run);
    
    // restore screen
    video_restore (savbuf);
    video_update (0);
}

/* ------------------------------------------------------------- */

static char *refine_line (char *s)
{
    static char buffer[512];
    int         i, j, n;

    // process tab characters
    for (i=0, n=0; i<sizeof(buffer)-1; i++, n++)
    {
        if (s[i] == '\0') break;
        if (s[i] == '\t')
        {
            for (j=n; j<(n/display.tabsize+1)*display.tabsize; j++)
                buffer[j] = ' ';
            n = j-1;
        }
        else
            buffer[n] = s[i];
    }
    
    buffer[n] = '\0';

    return buffer;
}

/* ------------------------------------------------------------- */

char *mon2txt (int month)
{
    switch (month)
    {
        case 0:  return MSG(M_JAN); break;
        case 1:  return MSG(M_FEB); break;
        case 2:  return MSG(M_MAR); break;
        case 3:  return MSG(M_APR); break;
        case 4:  return MSG(M_MAY); break;
        case 5:  return MSG(M_JUN); break;
        case 6:  return MSG(M_JUL); break;
        case 7:  return MSG(M_AUG); break;
        case 8:  return MSG(M_SEP); break;
        case 9:  return MSG(M_OCT); break;
        case 10: return MSG(M_NOV); break;
        case 11: return MSG(M_DEC); break;
    }
    return "???";
}


/* ------------------------------------------------------------- */

#define cm(a,b) ((a)==(b) ? 0 : ((a)>(b) ? 1 : -1))

int file_compare (__const__ void *elem1, __const__ void *elem2)
{
    file *f1 = (file *) elem1, *f2 = (file *) elem2;
    int   c_dir, c_name, c_ext, c_size, c_time;
    int   w_dir = 0, w_name = 0, w_ext = 0, w_size = 0, w_time = 0;
    long  c;

    if (strcmp (f1->name, "..") == 0) return -1;
    if (strcmp (f2->name, "..") == 0) return 1;
    
    w_dir = 10000;
    switch (sort.mode)
    {
    case SORT_UNSORTED: 
        return cm (f1->no, f2->no);
    case SORT_NAME:     
        w_name = 1000; w_ext = 100;   w_size = 0;     w_time = 10;  break;
    case SORT_EXT:      
        w_name = 100;  w_ext = 1000;  w_size = 0;     w_time = 10;   break;
    case SORT_SIZE:     
        w_name = 100;  w_ext = 0;     w_size = -1000; w_time = 10;   break;
    case SORT_TIME:
        w_name = 100;   w_ext = 10;   w_size = 0;    w_time = 1000; break;
    }
    
    c_dir = cm ((f1->flags & FILE_FLAG_DIR ? 1 : 0),
                 (f2->flags & FILE_FLAG_DIR ? 1 : 0));
    c = stricmp1 (f1->name, f2->name);
    c_name = cm (c, 0);
    c = stricmp1 (f1->name+f1->extension, f2->name+f2->extension);
    c_ext = cm (c, 0);
    c_size = cm (f1->flags & FILE_FLAG_DIR ? 0 : f1->size,
                 f1->flags & FILE_FLAG_DIR ? 0 : f2->size);
    c = f1->mtime - f2->mtime;
    c_time = cm (c, 0);
    
    return - w_dir*c_dir + sort.direction *
        (w_name*c_name + w_ext*c_ext + w_size*c_size + w_time*c_time);
}

/* ------------------------------------------------------------- */

void choose_local_drive (void)
{
    unsigned long   drivemap;
    int             drives[26], ndrives = 0;
    int             i, pointer = 0, oldpointer, disk = -2, key;
    int             r1, c1, r2, c2, cols;
    char            ch, buffer[MAX_PATH];

    local.drive = query_drive ();
    drivemap = query_drivemap ();

    // compute number of existing drives
    for (i=0; i<26; i++)
        if ((drivemap >> i) & 1)
        {
            if (local.drive == i) pointer = ndrives;
            drives[ndrives++] = i;
        }

    do
    {
        cols = ndrives*3 + 3;
        r1 = (video_vsize()-5)/2;
        r2 = r1 + 5;
        c1 = (video_hsize()-cols-2)/2;
        c2 = c1 + cols + 2 - 1;
    
        video_clear (r1, c1, r2, c2, fl_clr.dbox_back);
        fly_drawframe (r1, c1, r2, c2);
        video_put (MSG(M_CHOOSEDRIVE), r1+1, c1 + (c2-c1-12)/2);
        for (i=0; i<ndrives; i++)
        {
            ch = 'A'+drives[i];
            video_put_n_char (ch, 1, r1+3, c1+3+i*3);
        }

        video_put_n_attr (fl_clr.dbox_sel, 3, r1+3, c1+2+pointer*3);
        video_update (0);
        key = getmessage (-1);
        oldpointer = pointer;
        if (IS_KEY (key))
        {
            switch (key)
            {
            case _Esc   : disk = local.drive; break;
            case _Home  : pointer = 0; break;
            case _End   : pointer = ndrives-1; break;
            case _Left  : pointer = pointer == 0 ? ndrives-1 : pointer-1; break;
            case _Right : pointer = pointer == ndrives-1 ? 0 : pointer+1; break;
            case _Enter : disk = drives[pointer]; break;
            default     : if (key>='a' && key<='z' && drivemap >> (key-'a') & 1)
                disk = key - 'a';
            else if (key>='A' && key<='Z' && drivemap >> (key-'A') & 1)
                disk = key - 'A';
            }
        }
        if (IS_MOUSE (key))
        {
            switch (MOU_EVTYPE (key))
            {
            case MOUEV_B1SC:
                if (MOU_Y(key) != r1+3) break;
                for (i=0; i<ndrives; i++)
                    if (MOU_X(key) == c1+3+i*3) disk = i;
                break;
                
            case MOUEV_B2SC:
                disk = local.drive;
                break;
            }
        }
        if (oldpointer != pointer)
        {
            video_put_n_attr (fl_clr.dbox_back, 3, r1+3, c1+2+oldpointer*3);
            video_update (0);
        }

        if (disk != -2)
        {
            // check whether new drive has current directory (empty floppy
            // drives don't)
            if (change_drive (disk) ||
                getcwd (buffer, sizeof (buffer)) == NULL)
            {
                change_drive (local.drive);
                disk = -2;
            }
        }
    }
    while (disk == -2);
}

/* ------------------------------------------------------------- */

int save_listing (char *filename)
{
    int            i;
    FILE           *fh;
    int64_t        ts;
    char           *url;

    fh = fopen (filename, "w");
    if (fh == NULL)
    {
        fly_ask_ok (ASK_WARN, MSG(M_CANNOT_OPEN), filename);
        return -1;
    }

    url = compose_url (site.u, "", 0, 0);
    fprintf (fh, MSG(M_LISTING_HEADER), site.u.hostname, RCURDIR.name);
    fprintf (fh, "URL: %s\n\n", url);
    
    ts = 0;
    for (i=0; i<RNFILES; i++)
    {
        fputs (RFILE(i).rawtext, fh);
        fputs ("\n", fh);
        if (RFILE(i).flags & FILE_FLAG_REGULAR) ts += RFILE(i).size;
    }
    fprintf (fh, MSG(M_LISTING_FOOTER), RNFILES, pretty64(ts));
    
    fclose (fh);

    fly_ask_ok (0, MSG(M_LISTING_SAVED));
    return 0;
}

/* ------------------------------------------------------------- */

void set_local_path (char *path)
{
    int    newdrive;
    char   buffer[MAX_PATH];
    
    // try to set default drive
    if (path[1] == ':')
    {
        // extract drive from path string
        newdrive = path[0];
        if (newdrive <= 'Z' && newdrive >= 'A')
            newdrive = newdrive - 'A';
        else
            if (newdrive <= 'z' && newdrive >= 'a')
                newdrive = newdrive - 'a';
            else
                newdrive = 2; /* C: */

        if (newdrive < 26  && newdrive >= 0)
        {
            if (change_drive (newdrive) ||
                getcwd (buffer, sizeof (buffer)) == NULL)
            {
                fly_ask_ok (ASK_WARN, MSG(M_CANT_ACCESS_DRIVE), path[0]);
                return;
            }
        }
        local.drive = newdrive;
    }

    if (path[0] == '\0') return;
    if (chdir (path))
        fly_ask_ok (ASK_WARN, MSG(M_DIRECTORY_DOESNT_EXIST), path);
}

/* ------------------------------------------------------------- */

#define NEWLINE    0x0A
#define CARRIAGE   0x0D

void browse_buffer (char *s, int len, char *title)
{
   int     i, numlines;
   char    *buffer, *p1, **strpointers;

   buffer  = malloc ((size_t) len+3);
   memcpy (buffer, s, len);
   buffer[len] = 0;

   // replace all NULLs with spaces
   for (i=0; i<len; i++)
       if (buffer[i] == '\0') buffer[i] = ' ';

   len = str_refine_buffer (buffer);
   
   p1 = buffer;
   numlines = 0;
   while (p1-buffer < len) 
   {
       if (*p1 == '\0') numlines++;
       p1++;
   }

   strpointers = malloc (sizeof(void *) * (numlines+3));

   p1 = buffer;
   for (i=0; i<numlines; i++)
   {
       strpointers[i] = p1;
       while (*p1 != '\0') p1++;
       p1++;
   }
   
   ViewArray (strpointers, numlines, title);

   free (buffer);
   free (strpointers);
}

/* ------------------------------------------------------------- */

void put_time (unsigned long time, int row, int col)
{
    video_put_str (make_time (time), 9, row, col);
}

/* ------------------------------------------------------------- */

FILE *try_open (char *name, char *flags)
{
   int    i = 0;
   FILE   *descriptor;

   for (i=0; i<=4; i++)
   {
      descriptor = fopen (name, flags);
      if (descriptor != NULL) return descriptor;
      sleep1 (1.0);
   }
   return NULL;
}

/* ------------------------------------------------------------- */

void Bell (int melody)
{
    switch (melody)
    {
    case 1 : // file exists
        beep2 (300, 100);
        break;
        
    case 2 :  // transfer complete
        beep2 (500, 100);
        break;
        
    case 3 : // logged in
        beep2 (700, 100);
        break;

    case 4:
        beep2 (1000, 400);
        break;
    }
}

/*--------------------------------------------------------------------*/

void tmpnam1 (char *s)
{
    char        *p, buf[32];
    static int  n = 0;

    if (!fl_opt.has_driveletters)
        p = "/tmp";
    else
    {
        p = getenv ("TMPDIR");
        if (p == NULL) p = getenv ("TMP");
        if (p == NULL) p = getenv ("TEMP");
        if (p == NULL) p = ".";
        str_strip (p, " ");
        if (p[0] != '\0' && access (p, F_OK) != 0) p = ".";
    }
    
    do
    {
        strcpy (s, p);
        str_translate (s, '\\', '/');
        snprintf1 (buf, sizeof(buf), "nftp%04u.%03u", (int)(getpid()%10000), n++);
        str_cats (s, buf);
    }
    while (access (s, F_OK) == 0);
    
    str_translate (s, '\\', '/');
}

/* ------------------------------------------------------------- */

int breakflags (char *s)
{
    int flags = 0;
    
    if (strchr (s, 'f')) flags |= URL_FLAG_FIREWALLED;
    if (strchr (s, 'p')) flags |= URL_FLAG_PASSIVE;
    if (strchr (s, 'r')) flags |= URL_FLAG_RETRY;
    if (strchr (s, 'd')) flags |= URL_FLAG_SKEY;
    
    return flags;
}

/* ------------------------------------------------------------- */

char *makeflags (int flags)
{
    static char fl[16];

    fl[0] = '\0';
    if (flags & URL_FLAG_FIREWALLED) strcat (fl, "f");
    if (flags & URL_FLAG_PASSIVE) strcat (fl, "p");
    if (flags & URL_FLAG_RETRY) strcat (fl, "r");
    if (flags & URL_FLAG_SKEY) strcat (fl, "d");

    return fl;
}

/* ------------------------------------------------------------------ */

static char *int2char (unsigned int byte)
{
    static char s[4];
    
    s[0] = '0' + byte / 100;
    s[1] = '0' + (byte / 10) % 10;
    s[2] = '0' + byte % 10;
    s[3] = '\0';
    return s;
}

/* ------------------------------------------------------------------ */

static int char2int (char *s)
{
    return
        ((unsigned)s[0] - '0')*100 +
        ((unsigned)s[1] - '0')*10 +
        ((unsigned)s[2] - '0');
}

/* scrambles text string applying ? to it.
 returns malloc()ed buffer with scrambled version.
 */
char *scramble (char *s)
{
    char           *p;
    unsigned       uc, l, i, salt;

    // generate random salt
    salt = rand () % 1000;
    
    l = strlen (s);
    p = malloc (l*3 + 3 + 1);
    p[0] = '\0';
    for (i=0; i<l; i++)
    {
        uc = ((unsigned) (s[i])) + (salt*(i+1))%700;
        strcat (p, int2char (uc));
    }

    strcat (p, int2char(salt));
    
    return p;
}

/* unscrambles text string applying ? to it.
 returns malloc()ed buffer with unscrambled version.
 */
char *unscramble (char *s)
{
    char       *p;
    unsigned   l, i, salt;

    l = strlen (s);
    p = malloc (l/3 + 1);
    p[0] = '\0';
    if (l % 3 != 0 || s[0] == '\0') return p;

    salt = char2int (s+l-3);
    for (i=0; i<l/3-1; i++)
    {
        p[i] = char2int(s+i*3) - (salt*(i+1))%700;
    }
    p[i] = '\0';

    return p;
}

/* ------------------------------------------------------------- */

int mle2 (int numfields, entryfield_t *f, char *help1, int help2)
{
    unsigned   r1, r2, c1, c2, l3, l4, i, field, maxtitle, k, c;
    int        reply;
    char       *savebuf, buffer[128];
    
    // save screen before putting text
    savebuf = video_save (0, 0, video_vsize()-1, video_hsize()-1);

    // produce Yes/No strings
    if (yes1 == NULL)
    {
        yes1 = strdup (MSG(M_YES));
        str_strip2 (yes1, " ");
    }
    if (no1 == NULL)
    {
        no1 = strdup (MSG(M_NO));
        str_strip2 (no1, " ");
    }
    
    // compute max. title size
    maxtitle = 0;
    for (i=0; i<numfields; i++)
        if (f[i].type != EF_SEPARATOR)
            maxtitle = max1 (maxtitle, strlen (f[i].banner));
    
    field = 0;
    while (f[field].type == EF_SEPARATOR) field++;
    reply = -1000;
    do
    {
        // compute dialog size
        r2 = video_vsize() - 4;
        r1 = r2 - numfields - 1;
        c1 = 4;
        c2 = video_hsize() - 5;
        l3 = min1 (maxtitle, video_hsize()-10); // length of title
        l4 = c2-c1+1-l3-2-2; // length of input field

        // clear it and draw box
        video_clear (r1, c1, r2, c2, fl_clr.dbox_back);
        fly_drawframe (r1, c1, r2, c2);

        // draw current field contents
        for (i=0; i<numfields; i++)
        {
            if (f[i].type == EF_SEPARATOR)
            {
                video_put_n_char (fl_sym.h, c2-c1-1, r1+i+1, c1+1);
                video_put_n_char (fl_sym.t_l, 1, r1+i+1, c1);
                video_put_n_char (fl_sym.t_r, 1, r1+i+1, c2);
            }
            else
            {
                video_put_str (f[i].banner, min1 (l3, strlen (f[i].banner)),
                               r1+i+1, c1+2);
                if (f[i].type == EF_INTEGER || f[i].type == EF_STRING)
                    video_put_n_attr (fl_clr.entryfield, l4, r1+i+1, c1+2+l3+1);
                compose_buffer (&(f[i]), buffer, TRUE);
                video_put_str (buffer, min1 (l4, strlen (buffer)),
                               r1+i+1, c1+2+l3+1);
            }
        }
        video_put (help1, r2, c1+2);
        video_update (0);

        switch (f[field].type)
        {
        case EF_STRING:
        case EF_INTEGER:
            compose_buffer (&(f[field]), buffer, FALSE);
            reply = lineinput (buffer, sizeof (buffer), l4, fl_clr.entryfield,
                               r1+1+field, c1+2+l3+1,
                               f[field].flags|LI_RECOGNIZE_TABS|LI_USE_ARROWS, help2);
            break;
            
        case EF_BOOLEAN:
            reply = -1000;
            c = video_cursor_state (1);
            do
            {
                compose_buffer (&(f[field]), buffer, FALSE);
                video_put_str (buffer, min1 (l4, strlen (buffer)), r1+field+1, c1+2+l3+1);
                video_set_cursor (r1+field+1, f[field].value.boolean ? c1+2+l3+1+1 : c1+2+l3+1+13);
                video_update (0);
                k = getmessage (-1);
                if (IS_KEY(k))
                {
                    switch (k)
                    {
                    case _F1:
                        if (help2 != 0) help (help2);
                        break;
        
                    case _Esc :
                        reply = PRESSED_ESC;
                        break;
        
                    case _Enter :
                        reply = PRESSED_ENTER;
                        break;
        
                    case _Tab :
                    case _Down:
                        reply = PRESSED_TAB;
                        break;
        
                    case _ShiftTab :
                    case _Up:
                        reply = PRESSED_SHIFTTAB;
                        break;
        
                    case _Left:
                        if (!f[field].value.boolean) f[field].value.boolean = TRUE;
                        break;
        
                    case _Right:
                        if (f[field].value.boolean) f[field].value.boolean = FALSE;
                        break;
                    }
                }
            }
            while (reply == -1000);
            video_cursor_state (c);
        }
        
        // put edited value back
        switch (f[field].type)
        {
        case EF_INTEGER:
            f[field].value.integer = atoi (buffer);
            break;
        case EF_STRING:
            str_scopy (f[field].value.string, buffer);
            break;
        }

        switch (reply)
        {
        case PRESSED_ESC:
        case PRESSED_ENTER:
            break;
            
        case PRESSED_TAB:
            if (field == numfields-1) field = 0;
            else                      field++;
            while (f[field].type == EF_SEPARATOR && field < numfields) field++;
            break;

        case PRESSED_SHIFTTAB:
            if (field == 0) field = numfields - 1;
            else            field--;
            while (f[field].type == EF_SEPARATOR && field >= 0) field--;
        }
    }
    while (reply != PRESSED_ESC && reply != PRESSED_ENTER);
    
    video_restore (savebuf);
    video_update (0);
    return reply;
}

/* ------------------------------------------------------------- */

static void compose_buffer (entryfield_t *t, char *buffer, int hide)
{
    int  l;
    
    if (t->flags & LI_INVISIBLE && hide)
    {
        switch (t->type)
        {
        case EF_INTEGER: strcpy (buffer, "*"); break;
        case EF_STRING:  l = strlen (t->value.string); memset (buffer, '*', l); buffer[l] = '\0'; break;
        }
    }
    else
    {
        switch (t->type)
        {
        case EF_INTEGER:
            snprintf1 (buffer, 128, "%d", t->value.integer);
            break;
        case EF_STRING:
            strcpy (buffer, t->value.string);
            break;
        case EF_BOOLEAN:
            //strcpy (buffer, t->value.boolean ? "yes" : "no");
            snprintf1 (buffer, 128, "(%c) %6s  (%c) %s",
                     t->value.boolean ? 'X' : ' ', yes1, t->value.boolean ? ' ' : 'X', no1);
            break;
        }
    }
}

/* ------------------------------------------------------------- */

int mle (int numfields, char **banners, char **buffers, int flags, char *helpmsg)
{
    unsigned   r1, r2, c1, c2, i, field;
    int        reply;
    char       *savebuf;
    
    // save screen before putting text
    savebuf = video_save (0, 0, video_vsize()-1, video_hsize()-1);
    
    r2 = video_vsize()-4;
    r1 = r2 - numfields*3;
    c1 = 4;
    c2 = video_hsize()-5;
    
    video_clear (r1, c1, r2, c2, fl_clr.dbox_back);
    fly_drawframe (r1, c1, r2, c2);
    for (i=0; i<numfields; i++)
    {
        if (i != 0)
        {
            video_put (&fl_sym.t_l, r1+i*3, c1); video_put (&fl_sym.t_r, r1+i*3, c2);
            video_put_n_char (fl_sym.h, c2-c1-1, r1+i*3, c1+1);
        }
        video_put (banners[i], r1+i*3+1, c1+2);
        video_put_n_attr (fl_clr.entryfield, c2-c1+1-4, r1+i*3+2, c1+2);
        video_put (buffers[i], r1+i*3+2, c1+2);
    }
    video_put (helpmsg, r2, c1+2);
    video_update (0);
    
    field = 0;
    do {
        reply = lineinput (buffers[field], c2-c1+1-4, c2-c1+1-4,
                           fl_clr.entryfield,
                           r1+2+field*3, c1+2, flags|LI_RECOGNIZE_TABS, 0);

        if (reply == PRESSED_TAB)
        {
            if (field == numfields-1) field = 0;
            else                      field++;
        }
        
        if (reply == PRESSED_SHIFTTAB)
        {
            if (field == 0) field = numfields - 1;
            else field--;
        }
    } 
    while ((reply == PRESSED_TAB || reply == PRESSED_SHIFTTAB) &&
          reply != PRESSED_ESC);
    
    video_restore (savebuf);
    video_update (0);
    return reply;
}

/*
 Notes:
 1) cursor resides at the same location as buffer[cur_pos] points.
 when cur_pos=0, cursor stands at first string position
 when cur_pos=buffer_length-1, cursor stands at last buffer position
 2) last position (buffer_length-1) cannot be overwritten
 
 history:
 
 history[0]      -- always contains initial buffer
 history[1]      <- hist_cur
 history[2]
 history[3]
 history[4]      <- hist_num = 3
 */

int lineinput (char *buffer, int buffer_length, int vis_length,
               char attribute, int srow, int scol, int flags, int help1)
{
    int   i, m, k, l, l1, c_row, c_col, action, c;
    int   cur_pos, marked_attr, shift;
    int   insert_flag=TRUE, just_entered=TRUE, hist_cur = 0;
    char  *p, *p1;
    static char *history[32];
    static int  hist_num = 0;

    menu_disable ();
    
    /* fill in the area with an attribute */
    video_put_n_attr (attribute, vis_length, srow, scol);
    
    /* put an initial string with a corresponding attribute */
    marked_attr = attribute > 0x1f ? 0x07 : 0x70;
    
    /* prevent string from becoming unterminated */
    buffer[buffer_length-1] = 0;
    
    /* set up cursor */
    shift = 0;
    cur_pos = strlen (buffer);
    if (cur_pos > vis_length) cur_pos = 0;
    history[0] = NULL;
    action = -1;

    video_get_cursor (&c_row, &c_col);
    c = video_cursor_state (1);
    do
    {
        l  = strlen (buffer+shift);
        l1 = min1(l, vis_length);
        if (flags & LI_INVISIBLE)
            video_put_n_char ('*', l, srow, scol);
        else
            video_put_str (buffer+shift, l1, srow, scol);
        //video_put_n_char (MSG(M_PLACEHOLDER), vis_length-l1, srow, scol+l);
        video_put_n_char (' ', vis_length-l1, srow, scol+l);
        
        video_set_cursor (srow, scol+cur_pos-shift);
        video_update (0);
        k = getmessage (-1);
        if (IS_KEY(k))
        {
            switch (k)
            {
            case _F1:
                if (help1 != 0) help (help1);
                break;

            case _Esc :
                if (history[0] != NULL) free (history[0]);
                action = PRESSED_ESC;
                break;

            case _Enter :
                if (hist_num < 32-2 && buffer[0] != '\0' && !(flags & LI_INVISIBLE))
                {
                    history[hist_num+1] = strcpy (malloc (strlen (buffer)+1), buffer);
                    hist_num++;
                }
                if (history[0] != NULL) free (history[0]);
                action = PRESSED_ENTER;
                break;

            case _Tab :
                if (!(flags & LI_RECOGNIZE_TABS)) break;
                if (history[0] != NULL) free (history[0]);
                action = PRESSED_TAB;
                break;

            case _ShiftTab :
                if (!(flags & LI_RECOGNIZE_TABS)) break;
                if (history[0] != NULL) free (history[0]);
                action = PRESSED_SHIFTTAB;
                break;

            case _Left:
                if (cur_pos) cur_pos--;
                if (cur_pos < shift) shift = max1 (0, shift-1);
                break;

            case _Right:
                if (cur_pos < strlen (buffer)) cur_pos++;
                if (cur_pos-shift > vis_length) shift += 10;
                break;

            case _Up:
                if (flags & LI_USE_ARROWS)
                {
                    action = PRESSED_SHIFTTAB;
                    break;
                }
            case _CtrlUp:
                if (hist_cur == 0 && hist_num)
                {
                    if (history[0] != NULL) free (history[0]);
                    history[0] = strcpy (malloc (strlen (buffer)+1), buffer);
                    strncpy (buffer, history[1], buffer_length);
                    buffer[buffer_length-1] = '\0';
                    cur_pos = strlen (buffer);
                    hist_cur = 1;
                }
                else
                {
                    if (hist_cur >= hist_num) break;
                    strncpy (buffer, history[++hist_cur], buffer_length);
                    buffer[buffer_length-1] = '\0';
                    cur_pos = strlen (buffer);
                }
                break;

            case _Down:
                if (flags & LI_USE_ARROWS)
                {
                    action = PRESSED_TAB;
                    break;
                }
                
            case _CtrlDown:
                if (hist_cur == 0) break;
                strncpy (buffer, history[--hist_cur], buffer_length);
                buffer[buffer_length-1] = '\0';
                cur_pos = strlen (buffer);
                break;

            case _Home:
                cur_pos = 0;
                shift = 0;
                break;

            case _End:
                cur_pos = strlen (buffer);
                shift = max1 (0, cur_pos - vis_length + 5);
                break;

            case _BackSpace :
                if (just_entered)
                {
                    buffer[0] = 0;
                    cur_pos   = 0;
                    shift     = 0;
                    break;
                }
                if (!cur_pos) break;
                dmsg ("cur_pos = %d, shift = %d, vis_length = %d\n", cur_pos, shift, vis_length);
                m = --cur_pos;
                if (shift < cur_pos - vis_length - 1) shift = max1 (0, shift-1);
                shift = min1 (cur_pos, shift);
                while (buffer[m]) buffer[m] = buffer[m+1], m++;
                break;

            case _Insert :
                insert_flag = 1 - insert_flag;
                break;

            case _Delete :
                if (just_entered)
                {
                    buffer[0] = 0;
                    cur_pos   = 0;
                    shift     = 0;
                    break;
                }
                if (buffer[cur_pos] == 0) break;
                m = cur_pos;
                while (buffer[m]) buffer[m] = buffer[m+1], m++;
                break;

            case _ShiftInsert:
                p = get_clipboard ();
                if (p == NULL) break;
                p1 = p;
                // until CR/LF
                while (*p1 && *p1 != '\n' && *p1 != '\r') p1++;
                *p1 = '\0';
                strncpy (buffer, p, buffer_length);
                buffer[buffer_length-1] = '\0';
                cur_pos = strlen (buffer);
                shift = 0;
                free (p);
                break;

            case _CtrlInsert:
                put_clipboard (buffer);
                break;

            default :
                if (just_entered)
                {
                    buffer[0] = 0;
                    cur_pos   = 0;
                    shift     = 0;
                }
                if (k < 32 || k >= __EXT_KEY_BASE__) break;
                if (cur_pos == buffer_length-1) break;
                if (insert_flag)
                    for (i=buffer_length-1; i > cur_pos; i--)
                        buffer[i] = buffer[i-1];
                else
                    if (buffer[cur_pos] == 0) buffer[cur_pos+1] = 0;
                buffer[cur_pos] = (char) k;
                cur_pos++;
                if (cur_pos-shift > vis_length) shift+=10;
            }
            just_entered = FALSE;
        }

        if (IS_MOUSE(k))
        {
            switch (MOU_EVTYPE (k))
            {
            case MOUEV_B1DC:
                action = PRESSED_ENTER;
                break;
                
            case MOUEV_B2SC:
            case MOUEV_B3SC:
                p = get_clipboard ();
                if (p == NULL) break;
                p1 = p;
                // until CR/LF
                while (*p1 && *p1 != '\n' && *p1 != '\r') p1++;
                *p1 = '\0';
                strncpy (buffer, p, buffer_length);
                buffer[buffer_length-1] = '\0';
                cur_pos = strlen (buffer);
                shift = 0;
                break;
            }
        }
    }
    while (action == -1);
    
    video_cursor_state (c);
    video_set_cursor (c_row, c_col);
    video_update (0);
    
    menu_enable ();
    return action;
}

/* ------------------------------------------------------------- */

int entryfield (char *banner, char *buffer, int buf_len, int flags)
{
    unsigned   r1, r2, c1, c2;
    int        reply;
    char       *savebuf;
    
    r1 = video_vsize()-6;
    r2 = video_vsize()-4;
    c1 = 4;
    c2 = video_hsize()-4;
    
    // save screen before putting text
    savebuf = video_save (r1, c1, r2+1, c2+2);
    
    video_clear (r1, c1, r2, c2, fl_clr.dbox_back);
    fly_drawframe (r1, c1, r2, c2);
    video_put (banner, r1, c1+2);
    
    reply = lineinput (buffer, buf_len, c2-c1+1-4, fl_clr.entryfield,
                       r1+1, c1+2, flags, 0);
    
    video_restore (savebuf);
    video_update (0);
    return reply;
}

/* ------------------------------------------------------------- */

int is_absolute (char *path)
{
    if (!fl_opt.has_driveletters)
    {
        if (path[0] == '/') return TRUE;
    }
    else
    {
        if (path[0] == '/' || path[0] == '\\' || path[1] == ':') return TRUE;
    }
    return FALSE;
}

/* ------------------------------------------------------------- */

void dump_dirs (void)
{
    int i, j;
    
    dmsg ("ndirs = %d, current = %d\n", site.ndir, site.cdir);
    for (i=0; i<site.ndir; i++)
    {
        dmsg ("\n%d. Directory %s (%d files; current = %d, first = %d)\n",
              i, site.dir[i].name, site.dir[i].nfiles, site.dir[i].current_file,
              site.dir[i].first_item);
        for (j=0; j<site.dir[i].nfiles; j++)
        {
            dmsg ("%d. %s, %s bytes\n", j, site.dir[i].files[j].name,
                  printf64(site.dir[i].files[j].size));
        }
    }
}

/* -------------------------------------------------------------
 checks if string has 8 bit symbols
 */
int is_8bit (char *s)
{
    while (*s)
    {
        if (*((unsigned char *)s) > 128) return TRUE;
        s++;
    }
    return FALSE;
}

/* ------------------------------------------------------------- */

void set_marks (char *dirname)
{
    int   i, nd;
    char  *p;

    nd = find_directory (dirname);
    if (nd < 0) return;

    for (i=0; i<site.dir[nd].nfiles; i++)
    {
        if (site.dir[nd].files[i].flags & FILE_FLAG_REGULAR)
        {
            site.dir[nd].files[i].flags |= FILE_FLAG_MARKED;
        }

        if (site.dir[nd].files[i].flags & FILE_FLAG_DIR &&
           strcmp(site.dir[nd].files[i].name, ".."))
        {
            p = str_sjoin (site.dir[nd].name, site.dir[nd].files[i].name);
            set_marks (p);
            free (p);
            site.dir[nd].files[i].flags |= FILE_FLAG_MARKED;
        }
    }
}

/* ------------------------------------------------------------- */

void unset_marks (char *dirname)
{
    int    i, nd;
    char   *p;

    nd = find_directory (dirname);
    if (nd < 0) return;

    for (i=0; i<site.dir[nd].nfiles; i++)
    {
        if (site.dir[nd].files[i].flags & FILE_FLAG_REGULAR)
        {
            site.dir[nd].files[i].flags &= ~FILE_FLAG_MARKED;
        }

        if (site.dir[nd].files[i].flags & FILE_FLAG_DIR &&
           strcmp(site.dir[nd].files[i].name, ".."))
        {
            p = str_sjoin (site.dir[nd].name, site.dir[nd].files[i].name);
            unset_marks (p);
            free (p);
            site.dir[nd].files[i].flags &= ~FILE_FLAG_MARKED;
        }
    }
}

/* ------------------------------------------------------------- */

int is_walked (int nd, int nf)
{
    int   i, nd1;
    char  *p;

    // when file is given, consider it 'walked'
    if (!(site.dir[nd].files[nf].flags & FILE_FLAG_DIR)) return TRUE;
    if (strcmp(site.dir[nd].name, "..") == 0) return TRUE;

    p = str_sjoin (site.dir[nd].name, site.dir[nd].files[nf].name);
    nd1 = find_directory (p);
    free (p);
    if (nd1 == -1) return FALSE;

    for (i=0; i<site.dir[nd1].nfiles; i++)
    {
        if (site.dir[nd1].files[i].flags & FILE_FLAG_DIR &&
            strcmp(site.dir[nd1].files[i].name, ".."))
        {
            if (!is_walked (nd1, i)) return FALSE;
        }
    }

    return TRUE;
}

/* ------------------------------------------------------------- */
void adjust_cursor (directory *D, char *filename)
{
    int i;

    D->current_file = 0;
    if (filename == NULL || filename[0] == '\0') return;

    for (i=0; i<D->nfiles; i++)
    {
        if (!strcmp (D->files[i].name, filename))
        {
            D->current_file = i;
            //D->first_item = max1 (D->nf - p_nlf() + 1, 0);
        }
    }

    if (D->current_file < D->first_item)
    {
        D->first_item = max1 (0, D->current_file - p_nlf()/2);
    }
    else if (D->current_file - D->first_item > p_nlf()-1)
    {
        D->first_item = D->current_file - p_nlf() + 1;
    }
}

/* ------------------------------------------------------------- */

int localdir (directory *D, char *dirname)
{
    int                i, rc, nfa;
    DIR                *d;
    struct dirent      *e;
    struct stat        st;
    //struct passwd      *pwd;
    //struct group       *grp;
    char               *p;

    d = opendir (dirname);
    if (d == NULL) return -1;

    nfa = 64;
    D->files = malloc (sizeof(file) * nfa);

    for (i=0; ; )
    {
        if (i == nfa)
        {
            nfa *= 2;
            D->files = realloc (D->files, sizeof(file)*nfa);
        }

        e = readdir (d);
        if (e == NULL) break;
        if (!strcmp (e->d_name, ".")) continue;
        p = str_sjoin (dirname, e->d_name);
        rc = stat (p, &st);
        free (p);
        if (rc < 0) continue;

        D->files[i].rawtext = NULL;

        D->files[i].no = i;
        D->files[i].name = strdup (e->d_name);

        D->files[i].flags = 0;
        if (S_ISDIR(st.st_mode))
            D->files[i].flags |= FILE_FLAG_DIR;
        else
            D->files[i].flags |= FILE_FLAG_REGULAR;

        D->files[i].size   = st.st_size;
        D->files[i].mtime  = gm2local (max1 (0, st.st_mtime));
        D->files[i].rights = st.st_mode;
        D->files[i].desc = NULL;
        D->files[i].cached = NULL;

        //D->files[i].owner = NULL;
        //D->files[i].group = NULL;
        //if (fl_opt.is_unix)
        //{
        //    pwd = getpwuid (st.st_uid);
        //    if (pwd != NULL && pwd->pw_name != NULL)
        //        D->files[i].owner = strdup (pwd->pw_name);
        //    grp = getgrgid (st.st_gid);
        //    if (grp != NULL && grp->gr_name)
        //        D->files[i].group = strdup (grp->gr_name);
        //}

        p = strrchr (D->files[i].name, '.');
        if (p == NULL || p == D->files[i].name)
            D->files[i].extension = strlen (D->files[i].name);
        else
            D->files[i].extension = p + 1 - D->files[i].name;
        i++;
    }
    D->files = realloc (D->files, sizeof(file)*(i+2));

    D->nfiles = i;
    D->first_item = 0;
    D->current_file = 0;
    D->name = strdup (dirname);
    return 0;
}

/* ------------------------------------------------------------- */

void lcache_scandir (char *dirname)
{
    int   i, n;
    char  *p;

    if (lcache.lda == 0)
    {
        lcache.ld = 0;
        lcache.lda = 16;
        lcache.L = malloc (sizeof(directory) * lcache.lda);
    }
    if (lcache.lda == lcache.ld)
    {
        lcache.lda *= 2;
        lcache.L = realloc (lcache.L, sizeof(directory) * lcache.lda);
    }

    n = lcache.ld;
    if (localdir (&(lcache.L[n]), dirname) < 0) return;
    lcache.ld++;

    for (i=0; i<lcache.L[n].nfiles; i++)
    {
        if (lcache.L[n].files[i].flags & FILE_FLAG_DIR &&
            strcmp (lcache.L[n].files[i].name, ".."))
        {
            p = str_sjoin (dirname, lcache.L[n].files[i].name);
            lcache_scandir (p);
            free (p);
        }
    }
}

/* ------------------------------------------------------------- */

void lcache_free (void)
{
    int   i, j;

    // free cached local data
    for (i=0; i<lcache.ld; i++)
    {
        for (j=0; j<lcache.L[i].nfiles; j++)
        {
            free (lcache.L[i].files[j].name);
            if (lcache.L[i].files[j].desc != NULL) free (lcache.L[i].files[j].desc);
        }
        free (lcache.L[i].files);
    }

    if (lcache.L != NULL)
        free (lcache.L);

    lcache.ld = 0;
    lcache.lda = 0;
    lcache.L = NULL;
}

/* ------------------------------------------------------------- */
typedef struct
{
    int dir, file;
}
markedfile_t;

static markedfile_t *markedfile;
static int          nmarked;

static void drawline_marked (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[2048];
    int  length;
    file *f;

    f = site.dir[markedfile[n].dir].files + markedfile[n].file;
    snprintf1 (buf, sizeof(buf), "%11s %s/%s",
               f->flags & FILE_FLAG_DIR ? "<DIR>" : pretty64(f->size),
               site.dir[markedfile[n].dir].name, f->name);
    length = strlen (buf);

    video_put_n_char (' ', len, row, col);
    if (length > shift)
        video_put_str (buf+shift, length-shift, row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}

int do_review_marked (int action)
{
    int i, j, rc, nmarked_a;

    nmarked = 0;
    nmarked_a = 16;
    markedfile = xmalloc (sizeof(markedfile_t) * nmarked_a);

    /* gather marked files/directories */
    for (i=0; i<site.ndir; i++)
    {
        if (str_headcmp (site.dir[i].name, RCURDIR.name) != 0) continue;
        for (j=0; j<site.dir[i].nfiles; j++)
        {
            if (site.dir[i].files[j].flags & FILE_FLAG_MARKED)
            {
                if (nmarked == nmarked_a)
                {
                    nmarked_a *= 2;
                    markedfile = xrealloc (markedfile, sizeof(markedfile_t) * nmarked_a);
                }
                markedfile[nmarked].dir = i;
                markedfile[nmarked].file = j;
                nmarked++;
            }
        }
    }

    rc = fly_choose (" Selected files/directories on remote site ",
                     CHOOSE_ALLOW_HSCROLL, &nmarked, 0,
                     drawline_marked, NULL);

    free (markedfile);

    return 0;
}
