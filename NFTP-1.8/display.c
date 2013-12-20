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
#include "net.h"
#include "local.h"
#include "auxprogs.h"

struct _local local;
struct _local_cache lcache;

static void draw_line_remote (int n, int line);
static void draw_line_local  (int n, int line);
static void draw_panel (int r1, int c1, int r2, int c2,
                        int firstitem, int nfiles, int pointer_pos,
                        int has_pointer, file *F);
void display_menu_line (void);
void update_split_list (void);

/* ------------------------------------------------------------- */

void put_message (char *msg, ...)
{
   va_list       args;
   char          buffer[8192], *p, *p1;
   int           r1, r2, c1, c2, lines, cols, ch, r;

   va_start (args, msg);
   vsnprintf1 (buffer, sizeof(buffer), msg, args);
   va_end (args);

   //dmsg ("putting message %s\n", buffer);

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
#ifdef __EMX__
    dmsg ("update: _heapchk() = %d\n", _heapchk());
#endif
    if (status.split_view &&
        (display.view_mode == VIEW_REMOTE ||
         display.view_mode == VIEW_LOCAL))
    {
         update_split_list ();
    }
    else
    {
        switch (display.view_mode)
        {
        case VIEW_REMOTE:
            update_remote_list ();
            break;

        case VIEW_CONTROL:
            DisplayConnectionHistory ();
            break;

        case VIEW_LOCAL:
            update_local_list ();
            break;
        }
    }

    display_menu_line ();
    display_top_status_line ();
    display_bottom_status_line ();
    video_set_cursor (0, 0);
    if (flush)
        video_update (0);
}

/* ------------------------------------------------------------- */

void update_split_list (void)
{
    int    slp, row, lp;
    int    c1l, c2l, c1r, c2r, rs;

    // position of the left dividing line
    slp = video_hsize()/2 - 1;  // 39   (0-39, 40-79)

    /* ----------------------------------------------
     clear the screen and draw the frames
     ------------------------------------------------ */
    for (row=p_sl(); row<p_sl()+p_nl(); row++)
    {
        video_put_n_cell (' ', options.attr_tp_file, video_hsize(), row, 0);
        video_put_n_char (fl_sym.v, 2, row, slp);
        if (options.full_frames)
        {
            video_put_n_char (fl_sym.v, 1, row, 0);
            video_put_n_char (fl_sym.v, 1, row, video_hsize()-1);
        }
    }

    if (options.full_frames)
    {
        for (row=p_sl(); row<p_sl()+p_nl()-1; row++)
        {
            video_put_n_cell (' ', options.attr_tp_file, video_hsize(), row, 0);
            video_put_n_char (fl_sym.v, 2, row, slp);
            video_put_n_char (fl_sym.v, 1, row, 0);
            video_put_n_char (fl_sym.v, 1, row, video_hsize()-1);
        }
        video_put_n_char (fl_sym.h, video_hsize(), p_sl(), 0);
        video_put_n_char (fl_sym.h, video_hsize(), p_sl()+p_nl()-1, 0);
        video_put_n_char (fl_sym.c_lu, 1, p_sl(), 0);
        video_put_n_char (fl_sym.c_lu, 1, p_sl(), slp+1);
        video_put_n_char (fl_sym.c_ru, 1, p_sl(), slp);
        video_put_n_char (fl_sym.c_ru, 1, p_sl(), video_hsize()-1);
        video_put_n_char (fl_sym.c_ld, 1, p_sl()+p_nl()-1, 0);
        video_put_n_char (fl_sym.c_ld, 1, p_sl()+p_nl()-1, slp+1);
        video_put_n_char (fl_sym.c_rd, 1, p_sl()+p_nl()-1, slp);
        video_put_n_char (fl_sym.c_rd, 1, p_sl()+p_nl()-1, video_hsize()-1);
        c1l = 1;     c2l = slp-1;
        c1r = slp+2; c2r = video_hsize()-2;
        rs = p_sl()+p_nl()-1-1; // line where ministatus is displayed
    }
    else
    {
        for (row=p_sl(); row<p_sl()+p_nl(); row++)
        {
            video_put_n_cell (' ', options.attr_tp_file, video_hsize(), row, 0);
            video_put_n_char (fl_sym.v, 2, row, slp);
        }
        c1l = 0;     c2l = slp-1;
        c1r = slp+2; c2r = video_hsize()-1;
        rs = p_sl()+p_nl()-1;
    }

    if (status.show_ministatus)
    {
        video_put_n_char (fl_sym.h, c2l-c1l+1, rs-1, c1l);
        video_put_n_char (fl_sym.t_l, 1,           rs-1, c1l-1);
        video_put_n_char (fl_sym.t_r, 1,           rs-1, c2l+1);

        video_put_n_char (fl_sym.h, c2r-c1r+1, rs-1, c1r);
        video_put_n_char (fl_sym.t_l, 1,           rs-1, c1r-1);
        video_put_n_char (fl_sym.t_r, 1,           rs-1, c2r+1);
    }

    /* ----------------------------------------------
     draw files
     ------------------------------------------------ */
    draw_panel (p_slf(), c1l, p_slf()+p_nlf()-1, c2l,
                LFIRSTITEM, LNFILES, LCURFILENO, display.view_mode == VIEW_LOCAL, local.dir.files);
    if (!site.set_up)
    {
        lp = strlen (M("Not connected"));
        video_put_str_attr (M("Not connected"), lp, p_slf()+p_nlf()/2, slp+2+(video_hsize()-1-slp-2-lp)/2,
                            display.view_mode == VIEW_REMOTE ? options.attr_tp_file_p : options.attr_tp_file);
    }
    else
    {
        draw_panel (p_slf(), c1r, p_slf()+p_nlf()-1, c2r,
                    RFIRSTITEM, RNFILES, RCURFILENO, display.view_mode == VIEW_REMOTE, RCURDIR.files);
    }

    /* ----------------------------------------------
     draw ministatus
     ------------------------------------------------ */
    if (status.show_ministatus)
    {
        lp = min1 (strlen (LCURFILE.name), c2l-c1l+1);
        video_put_str (LCURFILE.name, lp, rs, c1l);
        if (site.set_up && RCURFILENO > 0)
        {
            lp = min1 (strlen (RCURFILE.name), c2l-c1l+1);
            video_put_str (RCURFILE.name, lp, rs, c1r);
        }
    }
}

/* ------------------------------------------------------------- */

static void draw_panel (int r1, int c1, int r2, int c2,
                        int firstitem, int nfiles, int pointer_pos,
                        int has_pointer, file *F)
{
    int    row, n, s1, s2, s3, lp;
    char   buf[64], at, *p;
    struct tm tm1;

/*            |12,000,000|21/12/98|12:30납                                      �
읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸 p_slf()+p_nlf()-1
0            s1         s2       s3slp1||slp2                                  |video_hsize()-1
 <---------->|<-------->|<------>|<--->|
                  10         8     5                         */
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

    // paint the lines
    for (row=r1; row<=r2; row++)
    {
        if (status.show_times)
        {
            video_put_n_char (fl_sym.v, 1, row, c1+s1);
            //video_put_n_char (fl_sym.v, 1, row, c1+s2);
            //video_put_n_char (fl_sym.v, 1, row, c1+s3);
        }
        else
        {
            if (options.hide_dates_too)
            {
                video_put_n_char (fl_sym.v, 1, row, c1+s1);
            }
            else
            {
                video_put_n_char (fl_sym.v, 1, row, c1+s1);
                //video_put_n_char (fl_sym.v, 1, row, c1+s2);
            }
        }

        n = firstitem + row-r1;
        if (n >= nfiles) continue;

        if (n == pointer_pos && has_pointer) /* pointer is here */
        {
            at = options.attr_pointer;
            if (F[n].flags & FILE_FLAG_MARKED)
            {
                if (F[n].flags & FILE_FLAG_DIR) at = options.attr_tp_dir__mp;
                else                            at = options.attr_tp_file_mp;
            }
            else
            {
                if (F[n].flags & FILE_FLAG_DIR) at = options.attr_tp_dir__p;
                else                            at = options.attr_tp_file_p;
            }
        }
        else  /* no pointer here */
        {
            if (F[n].flags & FILE_FLAG_MARKED)
            {
                if (F[n].flags & FILE_FLAG_DIR) at = options.attr_tp_dir__m;
                else                            at = options.attr_tp_file_m;
            }
            else
            {
                if (F[n].flags & FILE_FLAG_DIR) at = options.attr_tp_dir;
                else                            at = options.attr_tp_file;
            }
        }
        // paint the line
        video_put_n_attr (at, (c2-c1+1), row, c1);

        // name
        lp = strlen (F[n].name);
        video_put_str (F[n].name, min1 (s1-1, lp), row, c1);
        if (lp >= s1) video_put_n_char ('>', 1, row, c1+s1-1);

        // size
        if (F[n].flags & FILE_FLAG_DIR)
        {
            if (F[n].flags & FILE_FLAG_COMPSIZE)
            {
                snprintf1 (buf, sizeof(buf), "<%s>", pretty64 (F[n].size));
                p = buf;
            }
            else
            {
                p = M("<DIR>");
            }
            //lp = strlen (p);
            //video_put_str (p, min1 (lp, 12), row, c1+s2-min1 (lp, 12));
        }
        else if (F[n].flags & FILE_FLAG_LINK)
        {
            p = M("<LINK>");
            //lp = strlen (p);
            //video_put_str (p, min1 (lp, 12), row, c1+s2-min1 (lp, 12));
        }
        else
        {
            p = pretty64 (F[n].size);
            //lp = strlen (p);
            //video_put_str (p, lp, row, c1+s2-lp);
        }
        lp = strlen (p);
        video_put_str (p, lp, row, c1+s2-lp);
        tm1 = *localtime (&F[n].mtime);
        // date
        if (status.show_times || !options.hide_dates_too)
        {
            snprintf1 (buf, sizeof(buf), "%2d/%02d/%02d",
                     tm1.tm_mday, tm1.tm_mon+1, tm1.tm_year >= 100 ? tm1.tm_year-100 : tm1.tm_year);
            video_put_str (buf, 8, row, c1+s2+1);
        }
        // time
        if (status.show_times)
        {
            snprintf1 (buf, sizeof(buf), "%2d:%02d", tm1.tm_hour, tm1.tm_min);
            video_put_str (buf, 5, row, c1+s3+1);
        }
        // marked in monochrome mode?
        if (fl_opt.mono && (F[n].flags & FILE_FLAG_MARKED)) video_put_n_char ('*', 1, row, c1+s1);
    }

    // ties
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
}

/* ------------------------------------------------------------- */

void update_remote_list (void)
{
   int            nfile, nline, row0 = 0;

   if (site.set_up && site.ndir > 1)
   {
       for (nline=row0, nfile=RFIRSTITEM; nline<row0+p_nlf(); nline++, nfile++)
           draw_line_remote (nfile, nline);
   }
   else
   {
      for (nline=row0; nline<row0+p_nlf(); nline++)
      {
        video_put_n_cell (' ', options.attr_background, video_hsize(), nline+p_slf(), 0);
      }
   }
}

/* ------------------------------------------------------------- */

static void draw_line_remote (int n, int line)
{
   char           buf[1024], at, *marked, *size_field, *pd;
   int            time_distance, l1, l2, l3, ln, ld;
   struct tm      tm1;

   memset (buf, ' ', video_hsize());
   at = options.attr_;

   // bail out if this is empty line
   if (n >= RNFILES)
   {
       video_put_str_attr (buf, video_hsize(), line+p_slf(), 0, at);
       return;
   }

   // determine attribute
   if (n == RCURFILENO) /* pointer is here */
   {
       if (RFILE(n).flags & FILE_FLAG_MARKED)  /* tagged */
       {
           if (RFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_pointer_marked_dir;
           else if (RFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_pointer_marked;
           else
               at = options.attr_pointer_marked;
       }
       else                                     /* not tagged */
       {
           if (RFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_pointer_dir;
           else if (RFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_pointer;
           else
               at = options.attr_pointer;
       }
   }
   else  /* no pointer here */
   {
       if (RFILE(n).flags & FILE_FLAG_MARKED)  /* tagged */
       {
           if (RFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_marked_dir;
           else if (RFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_marked;
           else
               at = options.attr_marked;
       }
       else                                     /* not tagged */
       {
           if (RFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_dir;
           else if (RFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_;
           else
               at = options.attr_;
       }
   }
   // form line
   if (fl_opt.mono)
   {
       if (RFILE(n).flags & FILE_FLAG_MARKED)
           marked = "*";
       else
           marked = " ";
   }
   else
       marked = "";

   if (display.dir_mode == MODE_RAW)
   {
       if (display.rshift < strlen (RFILE(n).rawtext))
           strcpy (buf, RFILE(n).rawtext+display.rshift);
       video_put_str_attr (buf, video_hsize(), line+p_slf(), 0, at);
       return;
   }

   // draw a line with file data
   switch (display.dir_mode)
   {
   case MODE_PARSED:
       tm1 = *localtime (&RFILE(n).mtime);

       if ((RFILE(n).flags & FILE_FLAG_DIR) && (RFILE(n).flags & FILE_FLAG_COMPSIZE))
           size_field = pretty64 (RFILE(n).size);
       else if (RFILE(n).flags & FILE_FLAG_DIR)
           size_field = M("<DIR>");
       else if (RFILE(n).flags & FILE_FLAG_LINK)
           size_field = M("<LINK>");
       else
           size_field = pretty64 (RFILE(n).size);

       if (status.show_times)
       {
           time_distance = time(NULL) - RFILE(n).mtime;
           if (abs(time_distance) > 6*30*24*60*60)
           {
               snprintf1 (buf, sizeof(buf), "%3s %2d  %4d %13s %c %s",
                        //    mon day yea  sz | name
                        mon2txt (tm1.tm_mon),
                        tm1.tm_mday,
                        1900+tm1.tm_year,
                        size_field,
                        fl_sym.v, marked);
           }
           else
           {
               snprintf1 (buf, sizeof(buf), "%3s %2d %2d:%02d %13s %c %s",
                        //    mon day time     sz |  name
                        mon2txt (tm1.tm_mon),
                        tm1.tm_mday,
                        tm1.tm_hour,
                        tm1.tm_min,
                        size_field,
                        fl_sym.v, marked);
           }
           video_put_str_attr (buf, video_hsize(), line+p_slf(), 0, at);
       }
       else
       {
           snprintf1 (buf, sizeof(buf), "%13s %c %s",
                    //      sz | name
                    size_field,
                    fl_sym.v, marked);
           video_put_str_attr (buf, video_hsize(), line+p_slf(), 0, at);
       }
       break;
   }
   l1 = strlen (buf);

   // file name
   ln = strlen (RFILE(n).name);
   if (display.rshift < ln)
   {
       str_scopy (buf, RFILE(n).name);
       rcd_translate (buf, EN_WINDOWS, fl_opt.charset);
       // displayed size of name
       l2 = min1 (video_hsize()-l1, ln-display.rshift);
       //video_put_str_attr (RFILE(n).name+display.rshift, l2, line+p_slf(), l1, at);
       video_put_str_attr (buf+display.rshift, l2, line+p_slf(), l1, at);
   }
   else
       l2 = 0;

   // display description
   if (!status.load_descriptions) return;
   if (n >= RNFILES || RFILE(n).desc == NULL) return;
   if (display.dir_mode != MODE_PARSED) return;
   if (n == RCURFILENO) at = options.attr_pointer_desc;
   else                 at = options.attr_description;

   ld = strlen (RFILE(n).desc);
   str_scopy (buf, RFILE(n).desc);
   pd = strdup (RFILE(n).desc);
   if (is_8bit (pd) && options.guess_cyrillic)
       rcd_translate (buf, EN_UNKNOWN, fl_opt.charset);

   if (display.rshift < ln+1)
   {
       l3 = min1 (video_hsize()-l1-l2-2, ld);
       video_put_str_attr (pd, l3, line+p_slf(), l1+l2+2, at);
   }
   else if (display.rshift == ln+1 || display.rshift == ln+2)
   {
       l3 = min1 (video_hsize()-(l1+2+ln-display.rshift), ld);
       video_put_str_attr (pd, l3, line+p_slf(), l1+2+ln-display.rshift, at);
   }
   else
   {
       l3 = max1 (0, min1 (video_hsize()-l1, ld-(display.rshift-ln-2)));
       video_put_str_attr (pd+display.rshift-ln-2, l3, line+p_slf(), l1, at);
   }

   free (pd);
}

/* ------------------------------------------------------------- */

void sort_remote_files (char *bar_position)
{
    char *p = NULL;

    if (RNFILES == 0) return;

    if (bar_position != NULL)
        p = strdup (bar_position);
    else
        p = strdup (RCURFILE.name);

    sort.mode = site.sortmode;
    sort.direction = site.sortdirection;
    qsort (RCURDIR.files, RNFILES, sizeof(file), file_compare);

    if (p != NULL)
    {
        try_set_remote_cursor (p);
        free (p);
    }
    RFIRSTITEM = max1 (RCURDIR.current_file-p_nlf()+1, 0);
}

/* ------------------------------------------------------------- */

void try_set_remote_cursor (char *filename)
{
    int i;

    dmsg ("doing try_set_remote_cursor(%s)...\n", filename);
    RCURFILENO = 0;
    for (i=0; i<RNFILES; i++)
    {
        if (!strcmp (RFILE(i).name, filename))
        {
            RCURFILENO = i;
            RFIRSTITEM = max1 (RCURFILENO-p_nlf()+1, 0);
            //fix_screen_pos ();
            dmsg ("found (%s)\n", RCURFILE.name);
            return;
        }
    }
}

/* ------------------------------------------------------------- */

void update_local_list (void)
{
   char           buf[1024];
   int            nline, nfile;

   for (nline=0, nfile=LFIRSTITEM; nline < p_nlf(); nline++, nfile++)
       draw_line_local (nfile, nline);

   // bottom line
   memset (buf, ' ', video_hsize());
   if (fl_opt.has_driveletters)
   {
       buf[0] = local.drive + 'a';
       buf[1] = ':';   buf[2] = '/';
       strcpy (buf+2, local.dir.name);
   }
   else
       strcpy (buf, local.dir.name);

   video_put_str (buf, video_hsize(), video_vsize()-1, 0);
}

/* ------------------------------------------------------------- */

static void draw_line_local (int n, int line)
{
   char           buf[1024], at, *marked;
   int            time_distance;
   struct tm      tm1;

   memset (buf, ' ', video_hsize());
   at = options.attr_;

   // empty line?
   if (n >= LNFILES)
   {
       video_put_str_attr (buf, video_hsize(), line+p_slf(), 0, at);
       return;
   }

   if (n == LCURFILENO) /* pointer is here */
   {
       if (LFILE(n).flags & FILE_FLAG_MARKED)  /* tagged */
       {
           if (LFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_pointer_marked_dir;
           else if (LFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_pointer_marked;
           else
               at = options.attr_pointer_marked;
       }
       else                                     /* not tagged */
       {
           if (LFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_pointer_dir;
           else if (LFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_pointer;
           else
               at = options.attr_pointer;
       }
   }
   else  /* no pointer here */
   {
       if (LFILE(n).flags & FILE_FLAG_MARKED)  /* tagged */
       {
           if (LFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_marked_dir;
           else if (LFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_marked;
           else
               at = options.attr_marked;
       }
       else                                     /* not tagged */
       {
           if (LFILE(n).flags & FILE_FLAG_DIR)
               at = options.attr_dir;
           else if (LFILE(n).flags & FILE_FLAG_LINK)
               at = options.attr_;
           else
               at = options.attr_;
       }
   }

   // form line
   if (fl_opt.mono)
   {
       if (RFILE(n).flags & FILE_FLAG_MARKED)
           marked = "*";
       else
           marked = " ";
   }
   else
       marked = "";

   switch (display.dir_mode)
   {
   case MODE_RAW:
   case MODE_PARSED:
       tm1 = *localtime (&LFILE(n).mtime);
       time_distance = time(NULL) - LFILE(n).mtime;
       if (abs(time_distance) > 6*30*24*60*60)
       {
           snprintf1 (buf, sizeof(buf), "%3s %2d  %4d %13s %c %s%s",
                    //    mon day yea  sz | name
                    mon2txt (tm1.tm_mon),
                    tm1.tm_mday,
                    1900+tm1.tm_year,
                    LFILE(n).flags & FILE_FLAG_DIR ? M("<DIR>") :
                    pretty64 (LFILE(n).size),
                    fl_sym.v,
                    marked,
                    display.lshift < strlen (LFILE(n).name) ?
                    LFILE(n).name+display.lshift : "");
       }
       else
       {
           snprintf1 (buf, sizeof(buf), "%3s %2d %2d:%02d %13s %c %s%s",
                    //    mon day time     sz |  name
                    mon2txt (tm1.tm_mon),
                    tm1.tm_mday,
                    tm1.tm_hour,
                    tm1.tm_min,
                    LFILE(n).flags & FILE_FLAG_DIR ? M("<DIR>") :
                    pretty64 (LFILE(n).size),
                    fl_sym.v,
                    marked,
                    display.lshift < strlen (LFILE(n).name) ?
                    LFILE(n).name+display.lshift : "");
       }
   }

   video_put_str_attr (buf, video_hsize(), line+p_slf(), 0, at);
}

/* ------------------------------------------------------------- */

#define FILE_ANY	FILE_READONLY|FILE_HIDDEN|FILE_SYSTEM|FILE_DIRECTORY

void build_local_filelist (char *prevdir)
{
    DIR                *d;
    struct dirent      *e;
    struct stat        st;
    int                i, nmaxfiles = 100, prevmode;
    char               *p, buffer[2048];

again:
    local.drive = query_drive ();
    if (getcwd (buffer, sizeof(buffer)) == NULL)
    {
        strcpy (buffer, local.dir.name);
        goto go_up;
    }
    str_translate (buffer, '\\', '/');
    if (buffer[1] == ':')
        local.dir.name = chunk_add (buffer+2);
    else
        local.dir.name = chunk_add (buffer);

    if (local.dir.files != NULL) free (local.dir.files);
    local.dir.files = malloc (sizeof(file) * nmaxfiles);

    LNFILES = 0;
    LCURFILENO = 0;
    d = opendir (".");
    if (d == NULL) goto go_up;

    for (i=0; ; )
    {
        e = readdir (d);
        if (e == NULL) break;
        if (!strcmp (e->d_name, ".")) continue;
        if (stat (e->d_name, &st) < 0) continue;

        LFILE(i).rawtext = NULL;

        LFILE(i).no = i;
        LFILE(i).name = chunk_add (e->d_name);

        LFILE(i).flags = 0;
        if (S_ISDIR(st.st_mode))
            LFILE(i).flags |= FILE_FLAG_DIR;
        else
            LFILE(i).flags |= FILE_FLAG_REGULAR;

        LFILE(i).size   = st.st_size;
        LFILE(i).mtime  = st.st_mtime;
        LFILE(i).rights = st.st_mode;

        p = strrchr (LFILE(i).name, '.');
        if (p == NULL || p == LFILE(i).name)
            LFILE(i).extension = strlen (LFILE(i).name);
        else
            LFILE(i).extension = p + 1 - LFILE(i).name;
        i++;
        if (i == nmaxfiles-1)
        {
            local.dir.files = realloc (local.dir.files, sizeof(file)*nmaxfiles*2);
            nmaxfiles *= 2;
        }
    }
    if (i == 0) goto go_up;
    
    local.dir.files = realloc (local.dir.files, sizeof(file)*(i+2));
    
    LNFILES = i;
    LCURFILENO = 0;

    prevmode = display.view_mode;
    display.view_mode = VIEW_LOCAL;

    sort_local_files ();

    if (prevdir !=  NULL && !strncmp(prevdir, local.dir.name, strlen (local.dir.name)))
    {
       p = prevdir + strlen (local.dir.name);
       if (*p == '/') p++;
       try_set_local_cursor (p);
    }

    LFIRSTITEM = max1 (LCURFILENO-p_nlf()+1, 0);
    closedir (d);

    display.view_mode = prevmode;
    return;

fake_dir:
    // unreadable directory. create fake ".." entry
    LNFILES = 1;
    LCURFILENO = 0;
    LFILE(0).rawtext = NULL;
    LFILE(0).no = 0;
    LFILE(0).name = chunk_add ("..");
    LFILE(0).flags = FILE_FLAG_DIR;
    LFILE(0).size   = 0;
    LFILE(0).mtime  = 0;
    LFILE(0).rights = 0644;
    LFILE(0).extension = 2;
    local.dir.files = realloc (local.dir.files, sizeof(file)*1);
    LFIRSTITEM = 0;
    return;

go_up:
    while (TRUE)
    {
        /* error reading directory. most likely it was deleted. try to go up
         until succeed */
        p = strrchr (buffer, '/');
        if (p == NULL) goto fake_dir;
        *p = '\0';
        if (chdir (buffer) == 0) goto again;
    }
}

/* ------------------------------------------------------------- */

void sort_local_files (void)
{
    if (LNFILES == 0) return;
    sort.mode = local.sortmode;
    sort.direction = local.sortdirection;
    qsort (local.dir.files, LNFILES, sizeof(file), file_compare);
    LFIRSTITEM = 0;
    LCURFILENO = 0;
}

/* ------------------------------------------------------------- */

void try_set_local_cursor (char *filename)
{
    int i;

    LCURFILENO = 0;
    for (i=0; i<LNFILES; i++)
        if (!strcmp (LFILE(i).name, filename))
        {
            LCURFILENO = i;
            LFIRSTITEM = max1 (LCURFILENO-p_nlf()+1, 0);
            return;
        }
}

/* ------------------------------------------------------------- */

void fix_screen_pos (void)
{
    int   fl, nl;

    // check if the cursor below the first visible line
    if (RCURFILENO < RFIRSTITEM)
    {
        // position the cursor in the centre of the screen
        fl = RCURFILENO;
        nl = 0;
        while (fl > 0 && nl < p_nlf()-1)
        {
            fl--;
            //if (status.load_descriptions && display.dir_mode != MODE_PARSED
            //    && RFILE(fl).desc != NULL) nl++;
            nl++;
        }
        RFIRSTITEM = fl;
    }
    else
    {
        // if cursor is invisible, move first line down
        fl = RCURFILENO;
        nl = 0;
        while (fl > 0 && nl < p_nlf()-1)
        {
            fl--;
            //if (status.load_descriptions && display.dir_mode != MODE_PARSED
            //    && RFILE(fl).desc != NULL) nl++;
            nl++;
        }
        if (RFIRSTITEM < fl+1) RFIRSTITEM = fl;
    }
}

/* ------------------------------------------------------------- */

void display_bottom_status_line (void)
{
    char           buf[1024], stats[64];
    int            num_marked, num_total, attr, l, i;
    int64_t        size_marked, size_total;

    if (options.bottom_status_style == 0) return;

    if (options.bottom_status_style == 1) attr = options.attr_status2;
    else                                  attr = options.attr_status;

    num_marked = 0;    num_total = 0;
    size_marked = 0;   size_total = 0;

    switch (options.bottom_status_style)
    {
    case 2:
        video_put_n_cell (fl_sym.h, attr, video_hsize(), video_vsize()-2, 0);

    case 1:
        memset (buf, ' ', sizeof (buf));
        stats[0] = '\0';
        // statistics on the bottom status line when top status line is disabled
        if (options.top_status_style == 0)
        {
            if (display.view_mode == VIEW_REMOTE && site.set_up)
            {
                for (i=0; i<RNFILES; i++)
                {
                    if (!(RFILE(i).flags & FILE_FLAG_DIR))
                    {
                        if (RFILE(i).flags & FILE_FLAG_MARKED)
                        {
                            num_marked++;
                            size_marked += RFILE(i).size;
                        }
                        size_total += RFILE(i).size;
                        num_total++;
                    }
                }
            }
            if (display.view_mode == VIEW_LOCAL)
            {
                for (i=0; i<LNFILES; i++)
                {
                    if (!(LFILE(i).flags & FILE_FLAG_DIR))
                    {
                        if (LFILE(i).flags & FILE_FLAG_MARKED)
                        {
                            num_marked++;
                            size_marked += LFILE(i).size;
                        }
                        size_total += LFILE(i).size;
                        num_total++;
                    }
                }
            }
            if (display.view_mode == VIEW_REMOTE ||
                display.view_mode == VIEW_LOCAL)
            {
                snprintf1 (stats, sizeof(stats), "(%u%s/%u%s) %c%s%s ",
                         (int)(size_marked /
                         (size_marked < 10*1024 ? 1 :
                          (size_marked < 10*1024*1024 ? 1024 : 1024*1024))),

                         size_marked < 10*1024 ?  "" :
                         (size_marked > 10*1024*1024 ? "MB" : "KB"),

                         (int)(size_total /
                         (size_total < 10*1024 ? 1 :
                          (size_total < 10*1024*1024 ? 1024 : 1024*1024))),

                         size_total < 10*1024 ? "" :
                         (size_total > 10*1024*1024 ? "MB" : "KB"),
                         site.set_up ?
                         (status.binary ? 'I' : 'A') : ' ',
                         status.use_flags ? "F" : "",
                         status.use_proxy ? "P" : ""
                        );
            }
        }

        if (display.view_mode == VIEW_REMOTE || display.view_mode == VIEW_CONTROL)
        {
            if (site.set_up)
            {
                strcpy (buf+1, stats);
                strcat (buf, site.u.hostname);
                strcat (buf, ":");
                if (site.ndir > 1)
                {
                    // l is the place available for path
                    l = video_hsize() - strlen (buf);
                    if (l < strlen (RCURDIR.name))
                    {
                        strcat (buf, "...");
                        strcat (buf, RCURDIR.name+strlen(RCURDIR.name)-l+3+1);
                    }
                    else
                        strcat (buf, RCURDIR.name);
                }
            }
            else
            {
                snprintf1 (buf, sizeof(buf), M("NFTP%s Press Shift-H for general information"), NFTP_VERSION);
            }
        }
        if (display.view_mode == VIEW_LOCAL)
        {
            strcpy (buf+1, stats);
            l = strlen (buf);
            if (fl_opt.has_driveletters)
            {
                buf[l] = query_drive () + 'a';
                buf[l+1] = ':';
                buf[l+2] = '\0';
            }
            strcat (buf, local.dir.name);
        }
        video_put_n_cell (' ', attr, video_hsize(), video_vsize()-1, 0);
        video_put_str_attr (buf, video_hsize(), video_vsize()-1, 0, attr);
        if (stats[0] != '\0' && size_marked != 0)
            video_put_n_attr (options.attr_statmarked, strlen (stats)-3, video_vsize()-1, 1);
    }

    if (display.quicksearch[0] != '\0')
    {
        snprintf1 (buf, sizeof(buf), M("quick search: %s"), display.quicksearch);
        if (options.bottom_status_style == 1)
        {
            video_put_n_cell (' ', attr, video_hsize(), video_vsize()-1, 0);
            video_put (buf, video_vsize()-1, 0);
        }
        if (options.bottom_status_style == 2)
        {
            video_put (buf, video_vsize()-2, 4);
        }
    }
}

/* ------------------------------------------------------------- */

void display_top_status_line (void)
{
    char      buf[1024];
    int       i, attr, num_marked, num_total;
    int64_t   size_marked, size_total;

    if (options.top_status_style == 0) return;

    if (options.bottom_status_style == 1) attr = options.attr_status2;
    else                                  attr = options.attr_status;

    buf[0] = '\0';
    memset (buf, ' ', sizeof (buf));
    num_marked = 0;
    size_marked = 0;
    num_total = 0;
    size_total = 0;

    switch (display.view_mode)
    {
    case VIEW_REMOTE:
        if (site.set_up)
        {
            for (i=0; i<RNFILES; i++)
            {
                if (!(RFILE(i).flags & FILE_FLAG_DIR))
                {
                    if (RFILE(i).flags & FILE_FLAG_MARKED)
                    {
                        num_marked++;
                        size_marked += RFILE(i).size;
                    }
                    size_total += RFILE(i).size;
                    num_total++;
                }
            }
            snprintf1 (buf, sizeof(buf), M("Total %u file(s), %s bytes  [%3s]  Marked %u file(s), %s bytes"), num_total,
                     pretty64(size_total),
                     status.binary ? M("BIN") : M("ASC"),
                     num_marked, pretty64(size_marked));
        }
        else
        {
            strcpy (buf, M("Not connected"));
        }
        break;

    case VIEW_LOCAL:
        for (i=0; i<LNFILES; i++)
        {
            if (!(LFILE(i).flags & FILE_FLAG_DIR))
            {
                if (LFILE(i).flags & FILE_FLAG_MARKED)
                {
                    num_marked++;
                    size_marked += LFILE(i).size;
                }
                size_total += LFILE(i).size;
                num_total++;
            }
        }
        snprintf1 (buf, sizeof(buf), M("Total %u file(s), %s bytes  [%3s]  Marked %u file(s), %s bytes"), num_total,
                 pretty64(size_total),
                 status.binary ? M("BIN") : M("ASC"),
                 num_marked, pretty64(size_marked));
        break;

    case VIEW_CONTROL:
        memset (buf, ' ', sizeof (buf));
        snprintf1 (buf, sizeof(buf), M("Control Connection : %u lines"), cchistory.n);
    }

    switch (options.top_status_style)
    {
    case 2:
        video_put_n_cell (fl_sym.h, attr, video_hsize(), 1+fl_opt.menu_onscreen, 0);

    case 1:
        video_put_n_cell (' ', attr, video_hsize(), fl_opt.menu_onscreen, 0);
        video_put_str_attr (buf, video_hsize(), fl_opt.menu_onscreen, 0, attr);
    }
}

/* ------------------------------------------------------------- */

void display_menu_line (void)
{
    if (fl_opt.has_osmenu || !fl_opt.menu_onscreen) return;

    menu_process_line (main_menu, -2, 0);
}

/* -------------------------------------------------------------
 number of lines in the panel
 */

int p_nl (void)
{
    int n;

    n = video_vsize ();
    n -= options.bottom_status_style;
    n -= options.top_status_style;
    if (!fl_opt.has_osmenu && fl_opt.menu_onscreen) n--;

    return n;
}

/* -------------------------------------------------------------
 number of lines in the panel allocated for displaying files
 */

int p_nlf (void)
{
    int n;

    n = p_nl ();
    if (status.split_view && options.full_frames) n -= 2;
    if (status.split_view && status.show_ministatus) n -= 2;

    return n;
}

/* -------------------------------------------------------------
 rows of the first line in the panel
 */

int p_sl (void)
{
    int n;

    n = 0;
    n += options.top_status_style;
    if (!fl_opt.has_osmenu && fl_opt.menu_onscreen) n++;

    return n;
}

/* -------------------------------------------------------------
 rows of the first line in the panel allocated for displaying files
 */

int p_slf (void)
{
    int n;

    n = p_sl();
    if (status.split_view && options.full_frames) n++;

    return n;
}

/* ------------------------------------------------------------- */

#define FILE_ANY	FILE_READONLY|FILE_HIDDEN|FILE_SYSTEM|FILE_DIRECTORY

int l_chdir (char *newdir)
{
    //DIR                *d;
    //struct dirent      *e;
    //struct stat        st;
    int                i, /*nmaxfiles = 100,*/ rc;
    char               *p, buffer[2048], *oldpath, *newpath;
    directory          D;

    // if we are nowhere, find out where we are
    if (local.dir.name == NULL)
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
        local.dir.name = strdup (buffer);
    }

    // first we need to produce a new path (don't forget to save old one!)
    oldpath = strdup (local.dir.name);
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
            l_chdir ("/");
        }
        else
        {
            free (local.dir.name);
            local.dir.name = oldpath;
        }
        return -1;
    }
    else
    {
        for (i=0; i<local.dir.nfiles; i++)
            free (local.dir.files[i].name);
        if (local.dir.files != NULL) free (local.dir.files);
        local.dir = D;
    }
    sort_local_files ();

    if (oldpath != NULL && str_headcmp (oldpath, local.dir.name) == 0)
    {
        p = oldpath + strlen (local.dir.name);
        if (*p == '/') p++;
        adjust_cursor (&local.dir, p);
    }
    else
        adjust_cursor (&local.dir, NULL);

    LFIRSTITEM = max1 (LCURFILENO-p_nlf()+1, 0);

    if (oldpath != NULL)
        free (oldpath);

    return 0;
}
