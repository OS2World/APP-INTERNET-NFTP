#include <fly/fly.h>

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <manycode.h>
#include "nftp.h"
#include "keys.h"
#include "net.h"
#include "local.h"
#include "auxprogs.h"

struct _local       local[2];
struct _local_cache lcache;

static void panel_narrow (int r1, int c1, int r2, int c2, int pnl);
static void panel_wide (int r1, int r2, int pnl);
void display_menu_line (void);
void update_split_list (void);

/* ------------------------------------------------------------- */

void put_message (char *msg, ...)
{
   va_list       args;
   char          buffer[8192], *p, *p1;
   int           r1, r2, c1, c2, lines, cols, ch, r;

   va_start (args, msg);
   vsprintf (buffer, /*sizeof(buffer),*/ msg, args);
   va_end (args);

   p = buffer;    lines = 0;    cols = 0;   ch = 0;
   while (1)
   {
      if (*p=='\n' || *p==0)
      {
         cols = max1 (cols, ch);
         lines++;
         ch = 0;
      }
      else
         ch++;
      if (*p++ == 0) break;
   }
   lines = min1 (lines, video_vsize()-4);
   lines = max1 (1, lines);
   cols  = min1 (cols, video_hsize()-4);
   cols  = max1 (20, cols);

   // compute corner coordinates
   r1 = (video_vsize() - lines - 2)/2;
   r2 = r1 + lines + 2 - 1;
   c1 = (video_hsize() - cols - 2)/2;
   c2 = c1 + cols + 2 - 1;

   update (0);
   video_clear (r1, c1, r2, c2, fl_clr.dbox_back);
   fly_drawframe (r1, c1, r2, c2);

   // draw description lines
   p  = buffer;  p1 = p;   r = r1+1;
   while (1)
   {
      if (*p=='\n' || *p==0)
      {
         video_put_str (p1, min1(p-p1, video_hsize()-6), r++, c1+1);
         p1 = p + 1;
      }
      if (*p++ == 0) break;
   }
   video_update (0);
}

/* ------------------------------------------------------------- */

void update (int flush)
{
    int    slp;
    int    r1, r2, c1[2], c2[2];

    r1 = 0 + (fl_opt.menu_onscreen ? 1 : 0);
    r2 = video_vsize()-1 - 1;

    if (status.split_view)
    {
        // position of the left dividing line
        slp = video_hsize()/2 - 1;  // 39   (0-39, 40-79)
        c1[0] = 0;     c2[0] = slp;
        c1[1] = slp+1; c2[1] = video_hsize()-1;

        /* ----------------------------------------------
         draw file panels
         ------------------------------------------------ */
        panel_narrow (r1, c1[V_LEFT],  r2, c2[V_LEFT],  V_LEFT);
        panel_narrow (r1, c1[V_RIGHT], r2, c2[V_RIGHT], V_RIGHT);
    }
    else
    {
        panel_wide (r1, r2, display.cursor);
        /*
        if (display.view[display.cursor] < 0)
        {
            panel_wide (r1, r2, &(local[display.cursor].dir));
        }
        else
        {
            nsite = display.view[display.cursor];
            panel_wide (r1, r2, &(RN_CURDIR));
        }
        */
    }

    if (fl_opt.menu_onscreen)
        display_menu_line ();
    display_bottom_status_line ();
    video_set_cursor (0, 0);

    if (flush)
        video_update (0);
}

/*           |12,000,000|21/12/98|12:30납                                      �
읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸 p_slf()+p_nlf()-1
0            s1         s2       s3slp1||slp2                                  |video_hsize()-1
 <---------->|<-------->|<------>|<--->|
                  10         8     5                         */

static void panel_narrow (int r1, int c1, int r2, int c2, int pnl)
{
    int    row, n, s1, s2, s3, lp, has_pointer, l;
    char   buf[1024], at, *p;
    struct tm tm1;
    directory *D;

    if (display.view[pnl] < 0)
    {
        D = &local[pnl].dir;
    }
    else
    {
        D = &site[display.view[pnl]].dir[site[display.view[pnl]].cdir];
    }
    if (D == NULL) fly_error ("D=NULL in panel_narrow");
    has_pointer = display.cursor == pnl;

    //dmsg ("entering panel_narrow(r1=%d, c1=%d, r2=%d, c2=%d)\n", r1, c1, r2, c2);
    // clean the area
    for (row=r1; row<=r2; row++)
    {
        video_put_n_cell (' ', options.attr_tp_file, c2-c1+1, row, c1);
    }

    // paint frames
    if (options.full_frames)
    {
        // lines/corners
        for (row=r1; row<=r2; row++)
        {
            video_put_n_char (fl_sym.v, 1, row, c1);
            video_put_n_char (fl_sym.v, 1, row, c2);
        }
        video_put_n_char (fl_sym.h, c2-c1+1, r1, c1);
        video_put_n_char (fl_sym.h, c2-c1+1, r2, c1);
        video_put_n_char (fl_sym.c_lu, 1, r1, c1);
        video_put_n_char (fl_sym.c_ru, 1, r1, c2);
        video_put_n_char (fl_sym.c_ld, 1, r2, c1);
        video_put_n_char (fl_sym.c_rd, 1, r2, c2);
        r1++;  r2--;
    }
    c1++;  c2--;

    // compute coordinates
    if (status.show_times)
    {
        s1 = (c2-c1+1)-27;
        s2 = (c2-c1+1)-15;
        s3 = (c2-c1+1)-6;
    }
    else
    {
        if (options.hide_dates_too)
        {
            s1 = (c2-c1+1)-11;
            s2 = (c2-c1+1);
            s3 = (c2-c1+1);
        }
        else
        {
            s1 = (c2-c1+1)-21;
            s2 = (c2-c1+1)-9;
            s3 = (c2-c1+1);
        }
    }

    // paint place
    if (options.show_servername)
    {
        if (display.view[pnl] < 0)
        {
            p = local[pnl].dir.name;
        }
        else
        {
            //sprintf (buf, "%s : %s", site[display.view[pnl]].u.hostname,
            //         site[display.view[pnl]].dir[site[display.view[pnl]].cdir].name);
            //p = buf;
            if (site[display.view[pnl]].u.description[0] != '\0')
                p = site[display.view[pnl]].u.description;
            else
                p = site[display.view[pnl]].u.hostname;
        }
        lp = min1 (strlen(p), c2-c1+1);
        video_put_str_attr (p, lp, r1, c1+(c2-c1+1-lp)/2, options.attr_tp_header);
        video_put_n_char (fl_sym.h, c2-c1+1, r1+1, c1);
        if (options.full_frames)
        {
            video_put_n_char (fl_sym.t_l, 1, r1+1, c1-1);
            video_put_n_char (fl_sym.t_r, 1, r1+1, c2+1);
        }
        r1 += 2;
    }

    // mini-status
    if (status.show_ministatus)
    {
        video_put_n_char (fl_sym.h, c2-c1+1, r2-1, c1);
        if (options.full_frames)
        {
            video_put_n_char (fl_sym.t_l, 1, r2-1, c1-1);
            video_put_n_char (fl_sym.t_r, 1, r2-1, c2+1);
        }
        if (pnl == display.cursor && display.quicksearch[0] != '\0')
        {
            sprintf (buf, MSG(M_QUICK_SEARCH), display.quicksearch);
            video_put (buf, r2, c1);
        }
        else
        {
            lp = min1 (strlen (D->files[D->current].name), c2-c1+1);
            video_put_str (D->files[D->current].name, lp, r2, c1);
        }
        r2 -= 2;
    }

    // additional ties
    if (options.full_frames)
    {
        video_put_n_char (fl_sym.t_u, 1, r1-1, c1+s1);
        video_put_n_char (fl_sym.t_d, 1, r2+1, c1+s1);
        if (status.show_times || !options.hide_dates_too)
        {
            video_put_n_char (fl_sym.t_u, 1, r1-1, c1+s2);
            video_put_n_char (fl_sym.t_d, 1, r2+1, c1+s2);
        }
        if (status.show_times)
        {
            video_put_n_char (fl_sym.t_u, 1, r1-1, c1+s3);
            video_put_n_char (fl_sym.t_d, 1, r2+1, c1+s3);
        }
    }

    // paint headings
    if (options.show_panel_headings)
    {
        video_put_str_attr ("Name", strlen("Name"), r1, c1+(s1-4)/2, options.attr_tp_header);
        video_put_n_char (fl_sym.v, 1, r1, c1+s1);
        video_put_str_attr ("Size", strlen("Size"), r1, c1+s1+(s2-s1-4)/2+1, options.attr_tp_header);
        video_put_n_char (fl_sym.v, 1, r1, c1+s2);
        if (status.show_owner)
        {
            l = strlen ("Owner/Group");
            video_put_str_attr ("Owner/Group", l, r1, c1+s2+1, options.attr_tp_header);
        }
        else
        {
            if (status.show_times)
            {
                l = strlen("Date");
                video_put_str_attr ("Date", l, r1, c1+s2+(s3-s2-l)/2+1, options.attr_tp_header);
                video_put_n_char (fl_sym.v, 1, r1, c1+s3);
                video_put_str_attr ("Time", strlen("Time"), r1, c1+s3+1, options.attr_tp_header);
            }
            else
            {
                if (!options.hide_dates_too)
                {
                    video_put_str_attr ("Date", strlen("Date"), r1, c1+s2+(s3-s2-4)/2+1, options.attr_tp_header);
                    video_put_n_char (fl_sym.v, 1, r1, c1+s2);
                }
            }
        }
        r1++;
    }

    // paint the lines
    for (row=r1; row<=r2; row++)
    {
        video_put_n_char (fl_sym.v, 1, row, c1+s1);

        n = D->first + row-r1;
        if (n >= D->nfiles) continue;

        if (n == D->current && has_pointer) /* pointer is here */
        {
            if (D->files[n].flags & FILE_FLAG_MARKED)
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_tp_dir__mp;
                else
                    at = options.attr_tp_file_mp;
            }
            else
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_tp_dir__p;
                else
                    at = options.attr_tp_file_p;
            }
        }
        else  /* no pointer here */
        {
            if (D->files[n].flags & FILE_FLAG_MARKED)
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_tp_dir__m;
                else
                    at = options.attr_tp_file_m;
            }
            else
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_tp_dir;
                else
                    at = options.attr_tp_file;
            }
        }
        // paint the line
        video_put_n_attr (at, (c2-c1+1), row, c1);

        // name
        lp = strlen (D->files[n].name);
        video_put_str (D->files[n].name, min1 (s1, lp), row, c1);
        if (lp > s1) video_put_n_char ('>', 1, row, c1+s1-1);
        // size
        if (D->files[n].flags & FILE_FLAG_DIR)
        {
            if (D->files[n].flags & FILE_FLAG_COMPSIZE)
            {
                sprintf (buf, "<%s>", pretty64 (D->files[n].size));
                p = buf;
            }
            else
            {
                p = MSG(M_DIR_SIGN);
            }
            //lp = strlen (p);
            //video_put_str (p, min1 (lp, 12), row, c1+s2-min1 (lp, 12));
        }
        else if (D->files[n].flags & FILE_FLAG_LINK)
        {
            p = MSG(M_LINK_SIGN);
            //lp = strlen (p);
            //video_put_str (p, min1 (lp, 12), row, c1+s2-min1 (lp, 12));
        }
        else
        {
            p = pretty64 (D->files[n].size);
        }
        lp = strlen (p);
        video_put_str (p, lp, row, c1+s2-lp);
        // date
        if (status.show_owner)
        {
            sprintf (buf, "%s/%s",
                     D->files[n].owner == NULL ? "-" : D->files[n].owner,
                     D->files[n].group == NULL ? "-" : D->files[n].group);
            video_put (buf, row, c1+s2+1);
        }
        else
        {
            tm1 = *gmtime (&D->files[n].mtime);
            if (status.show_times || !options.hide_dates_too)
            {
                sprintf (buf, "%2d/%02d/%02d",
                         tm1.tm_mday, tm1.tm_mon+1, tm1.tm_year >= 100 ? tm1.tm_year-100 : tm1.tm_year);
                video_put_str (buf, 8, row, c1+s2+1);
            }
            // time
            if (status.show_times)
            {
                sprintf (buf, "%2d:%02d", tm1.tm_hour, tm1.tm_min);
                video_put_str (buf, 5, row, c1+s3+1);
            }
            // marked in monochrome mode?
            if (fl_opt.mono && (D->files[n].flags & FILE_FLAG_MARKED)) video_put_n_char ('*', 1, row, c1+s1);
        }
    }
}

/* ------------------------------------------------------------- */

static void panel_wide (int r1, int r2, int pnl)
{
    char           buf[1024], at, *marked, *size_field, *pd, *p;
    int            time_distance, l1, l2, l3, ln, ld, row, n, lp;
    int            c1, c2;
    struct tm      tm1;
    directory      *D;

    if (display.view[pnl] < 0)
    {
        D = &local[pnl].dir;
    }
    else
    {
        D = &site[display.view[pnl]].dir[site[display.view[pnl]].cdir];
    }
    if (D == NULL) fly_error ("D=NULL in panel_narrow");
    c1 = 0;
    c2 = video_hsize() - 1;

    // clean the area
    for (row=r1; row<=r2; row++)
    {
        video_put_n_cell (' ', options.attr_sp_file, c2-c1+1, row, c1);
    }

    // paint frames
    if (options.full_frames)
    {
        // lines/corners
        for (row=r1; row<=r2; row++)
        {
            video_put_n_char (fl_sym.v, 1, row, c1);
            video_put_n_char (fl_sym.v, 1, row, c2);
        }
        video_put_n_char (fl_sym.h, c2-c1+1, r1, c1);
        video_put_n_char (fl_sym.h, c2-c1+1, r2, c1);
        video_put_n_char (fl_sym.c_lu, 1, r1, c1);
        video_put_n_char (fl_sym.c_ru, 1, r1, c2);
        video_put_n_char (fl_sym.c_ld, 1, r2, c1);
        video_put_n_char (fl_sym.c_rd, 1, r2, c2);
        r1++;  r2--;
        c1++;  c2--;
    }

    // paint place
    if (options.show_servername)
    {
        if (display.view[pnl] < 0)
        {
            p = local[pnl].dir.name;
        }
        else
        {
            //sprintf (buf, "%s : %s", site[display.view[pnl]].u.hostname,
            //         site[display.view[pnl]].dir[site[display.view[pnl]].cdir].name);
            //p = buf;
            p = site[display.view[pnl]].u.hostname;
        }
        lp = min1 (strlen(p), c2-c1+1);
        video_put_str_attr (p, lp, r1, c1+(c2-c1+1-lp)/2, options.attr_tp_header);
        video_put_n_char (fl_sym.h, c2-c1+1, r1+1, c1);
        if (options.full_frames)
        {
            video_put_n_char (fl_sym.t_l, 1, r1+1, c1-1);
            video_put_n_char (fl_sym.t_r, 1, r1+1, c2+2);
        }
        r1 += 2;
    }
    // paint headings
    if (options.show_panel_headings)
    {
        if (display.parsed || display.view[pnl] < 0)
        {
            if (status.show_times)
            {
                video_put_str_attr ("Date", strlen ("Date"), r1, c1+1, options.attr_tp_header);
                video_put_n_char (fl_sym.v, 1, r1, c1+3);
                video_put_str_attr ("Time", strlen ("Time"), r1, c1+8, options.attr_tp_header);
                video_put_str_attr ("Size", strlen ("Size"), r1, c1+18, options.attr_tp_header);
                video_put_str_attr ("Name", strlen ("Name"), r1, c1+28, options.attr_tp_header);
            }
            else
            {
                video_put_str_attr ("Size", strlen ("Date"), r1, c1+18, options.attr_tp_header);
                video_put_str_attr ("Name", strlen ("Date"), r1, c1+28, options.attr_tp_header);
            }
        }
        else
        {
            video_put_str_attr ("FTP response line", strlen ("FTP response line"),
                                r1, c1+1, options.attr_tp_header);
        }
        r1++;
    }

    for (row=r1; row<=r2; row++)
    {
        n = row-r1 + D->first;

        // bail out if this is empty line
        if (n >= D->nfiles) continue;

       // determine attribute
        if (n == D->current) /* pointer is here */
        {
            if (D->files[n].flags & FILE_FLAG_MARKED)
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_sp_dir__mp;
                else
                    at = options.attr_sp_file_mp;
            }
            else
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_sp_dir__p;
                else
                    at = options.attr_sp_file_p;
            }
        }
        else  /* no pointer here */
        {
            if (D->files[n].flags & FILE_FLAG_MARKED)
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_sp_dir__m;
                else
                    at = options.attr_sp_file_m;
            }
            else
            {
                if (D->files[n].flags & FILE_FLAG_DIR)
                    at = options.attr_sp_dir;
                else
                    at = options.attr_sp_file;
            }
        }
        video_put_n_attr (at, c2-c1+1, row, c1);

        // form line
        if (fl_opt.mono)
        {
            if (D->files[n].flags & FILE_FLAG_MARKED)
                marked = "*";
            else
                marked = " ";
        }
        else
            marked = "";

        // unparsed view?
        if (!display.parsed && D->files[n].rawtext != NULL)
        {
            lp = min1 (strlen (D->files[n].rawtext) - display.rshift, c2-c1+1);
            if (lp <= 0) continue;
            video_put_str (D->files[n].rawtext+display.rshift, lp, row, c1);
            continue;
        }

       // draw a line with file data
       tm1 = *gmtime (&D->files[n].mtime);

       if ((D->files[n].flags & FILE_FLAG_DIR) && (D->files[n].flags & FILE_FLAG_COMPSIZE))
           size_field = pretty64 (D->files[n].size);
       else if (D->files[n].flags & FILE_FLAG_DIR)
           size_field = MSG(M_DIR_SIGN);
       else if (D->files[n].flags & FILE_FLAG_LINK)
           size_field = MSG(M_LINK_SIGN);
       else
           size_field = pretty64 (D->files[n].size);

       if (status.show_times)
       {
           time_distance = time(NULL) - D->files[n].mtime;
           if (abs(time_distance) > 6*30*24*60*60)
           {
               sprintf (buf, /*sizeof(buf),*/ "%3s %2d  %4d %13s %c %s",
                        //    mon day yea  sz | name
                        mon2txt (tm1.tm_mon),
                        tm1.tm_mday,
                        1900+tm1.tm_year,
                        size_field,
                        fl_sym.v, marked);
           }
           else
           {
               sprintf (buf, /*sizeof(buf),*/ "%3s %2d %2d:%02d %13s %c %s",
                        //    mon day time     sz |  name
                        mon2txt (tm1.tm_mon),
                        tm1.tm_mday,
                        tm1.tm_hour,
                        tm1.tm_min,
                        size_field,
                        fl_sym.v, marked);
           }
           lp = min1 (strlen(buf), c2-c1+1);
           video_put_str (buf, lp, row, c1);
       }
       else
       {
           sprintf (buf, "%13s %c %s",
                    //      sz | name
                    size_field,
                    fl_sym.v, marked);
           lp = min1 (strlen(buf), c2-c1+1);
           video_put_str (buf, lp, row, c1);
       }
       l1 = strlen (buf);

       // file name
       ln = strlen (D->files[n].name);
       if (display.rshift < ln)
       {
           // displayed size of name
           l2 = min1 (c2-c1+1-l1, ln-display.rshift);
           video_put_str (D->files[n].name+display.rshift, l2, row, l1);
       }
       else
           l2 = 0;

       // display description
       if (!status.load_descriptions || D->files[n].desc == NULL) continue;
       if (n == D->current) at = options.attr_sp_desc_p;
       else                 at = options.attr_sp_desc;

       ld = strlen (D->files[n].desc);
       pd = strdup (D->files[n].desc);
       if (is_8bit (pd) && options.guess_cyrillic) rcd_translate (pd, EN_UNKNOWN, fl_opt.charset);

       if (display.rshift < ln+1)
       {
           l3 = min1 (c2-c1+1-l1-l2-2, ld);
           video_put_str_attr (pd, l3, row, l1+l2+2, at);
       }
       else if (display.rshift == ln+1 || display.rshift == ln+2)
       {
           l3 = min1 (c2-c1+1-(l1+2+ln-display.rshift), ld);
           video_put_str_attr (pd, l3, row, l1+2+ln-display.rshift, at);
       }
       else
       {
           l3 = max1 (0, min1 (c2-c1+1-l1, ld-(display.rshift-ln-2)));
           video_put_str_attr (pd+display.rshift-ln-2, l3, row, l1, at);
       }

       free (pd);
   }
}

/* ------------------------------------------------------------- */

#define FILE_ANY	FILE_READONLY|FILE_HIDDEN|FILE_SYSTEM|FILE_DIRECTORY

int l_chdir (int pnl, char *newdir)
{
    //DIR                *d;
    //struct dirent      *e;
    //struct stat        st;
    int                i, /*nmaxfiles = 100,*/ rc;
    char               *p, buffer[2048], *oldpath, *newpath;
    directory          D;

    // if we are nowhere, find out where we are
    if (local[pnl].dir.name == NULL)
    {
        if (fl_opt.has_driveletters)
        {
            buffer[0] = query_drive () + 'A';
            buffer[1] = ':';
            getcwd (buffer+2, sizeof(buffer)-2);
        }
        else
        {
            getcwd (buffer, sizeof(buffer));
        }
        local[pnl].dir.name = strdup (buffer);
    }

    // first we need to produce a new path (don't forget to save old one!)
    oldpath = strdup (local[pnl].dir.name);
    if (newdir == NULL)
    {
        newpath = strdup (oldpath);
    }
    else
    {
        if (is_absolute (newdir))
        {
            p = strdup (newdir);
        }
        else
        {
            p = str_sjoin (oldpath, newdir);
        }
        newpath = str_sanify (p);
        free (p);
    }

    rc = localdir (&D, newpath);
    if (rc < 0)
    {
        // unreadable or nonexisting directory
        if (newdir == NULL)
        {
            // oops, old directory is unreadable!
            free (newpath);
            l_chdir (pnl, "/");
        }
        else
        {
            free (local[pnl].dir.name);
            local[pnl].dir.name = oldpath;
        }
        return -1;
    }
    else
    {
        for (i=0; i<local[pnl].dir.nfiles; i++)
            free (local[pnl].dir.files[i].name);
        if (local[pnl].dir.files != NULL) free (local[pnl].dir.files);
        local[pnl].dir = D;
    }
    sort_local_files (pnl);

    if (oldpath != NULL && !strncmp(oldpath, local[pnl].dir.name, strlen (local[pnl].dir.name)))
    {
        p = oldpath + strlen (local[pnl].dir.name);
        if (*p == '/') p++;
        adjust_cursor (&local[pnl].dir, p);
    }

    LFIRSTITEM(pnl) = max1 (LCURFILENO(pnl)-p_nlf()+1, 0);

    if (oldpath != NULL)
        free (oldpath);

    return 0;
}

/* ------------------------------------------------------------- */
void display_menu_line (void)
{
    if (fl_opt.has_osmenu || !fl_opt.menu_onscreen) return;

    menu_process_line (main_menu, -2, 0);
}

/* -------------------------------------------------------------
 number of lines in the panel allocated for displaying files */
int p_nlf (void)
{
    int n;

    n = video_vsize ();
    n -= 1; // bottom status line
    n -= 1; // heading

    if (options.full_frames) n -= 2;
    if (!fl_opt.has_osmenu && fl_opt.menu_onscreen) n--;
    if (status.split_view && status.show_ministatus) n -= 2;
    if (options.show_servername) n -= 2;

    return n;
}

/* -------------------------------------------------------------
ask_logoff:
asks whether user really want to log off. also checks for marked
files
returns: 1 - do not logoff
         0 - logoff
*/
int ask_logoff (int nsite)
{
    int     marked_no, marked_dirs;
    int64_t marked_size;

    if (!site[nsite].connected) return 0;
    if (!options.ask_logoff) return 0;
    gather_marked (nsite, 0, &marked_no, &marked_dirs, &marked_size);

    if (marked_no)
    {
        return !fly_ask (ASK_YN, MSG(M_LOGOFF_MARKED), NULL,
                         marked_no, marked_dirs, pretty64(marked_size));
    }
    else if (options.ask_exit)
    {
        return !fly_ask (ASK_YN, MSG(M_REALLY_LOGOFF), NULL, site[nsite].u.hostname);
    }
    else
    {
        return 0;
    }
}

/* ------------------------------------------------------------- */

void display_bottom_status_line (void)
{
    int  lp, nsite, i, ls;
    char *p, buffer[1024];
    int64_t      size_marked=0, size_total=0;
    unsigned int num_marked=0, num_total=0;

    video_put_n_cell (' ', options.attr_status2, video_hsize(), video_vsize()-1, 0);

    if (display.view[display.cursor] < 0)
    {
        for (i=0; i<LNFILES(display.cursor); i++)
        {
            if (!(LFILE(display.cursor, i).flags & FILE_FLAG_DIR))
            {
                if (LFILE(display.cursor, i).flags & FILE_FLAG_MARKED)
                {
                    num_marked++;
                    size_marked += LFILE(display.cursor, i).size;
                }
                size_total += LFILE(display.cursor, i).size;
                num_total++;
            }
        }
    }
    else
    {
        nsite = display.view[display.cursor];
        for (i=0; i<RN_NF; i++)
        {
            if (!(RN_FILE(i).flags & FILE_FLAG_DIR))
            {
                if (RN_FILE(i).flags & FILE_FLAG_MARKED)
                {
                    num_marked++;
                    size_marked += RN_FILE(i).size;
                }
                size_total += RN_FILE(i).size;
                num_total++;
            }
        }
    }

    sprintf (buffer, " %u files, %u %s ",
             num_total,
             (int)(size_total /
                   (size_total < 10*1024 ? 1 :
                    (size_total < 10*1024*1024 ? 1024 : 1024*1024))),

             size_total < 10*1024 ? "B" :
             (size_total > 10*1024*1024 ? "MB" : "KB")
            );
    ls = min1 (strlen(buffer), video_hsize()-1);
    video_put_str (buffer, ls, video_vsize()-1, 0);

    if (display.view[display.cursor] < 0)
    {
        p = local[display.cursor].dir.name;
    }
    else
    {
        nsite = display.view[display.cursor];
        sprintf (buffer, "%s:%s", site[nsite].u.hostname, RN_CURDIR.name);
        p = buffer;
    }
    lp = min1 (strlen(p), video_hsize()-3-ls);
    video_put_str (p, lp, video_vsize()-1, ls);
}
