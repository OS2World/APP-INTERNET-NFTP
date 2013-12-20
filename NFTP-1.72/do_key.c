/* I think Total Commander (nee' Windows Commander) from www.ghisler.com
has this right: when a file exists in the destination, it presents
a dialog with lots of buttons:

[Overwrite]
[Overwrite all]
[Overwrite all older]
[Skip]
[Skip all]
[Skip all older]
[Rename]
[Cancel]
*/

#include <fly/fly.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "nftp.h"
#include "keys.h"
#include "net.h"
#include "local.h"
#include "history.h"
#include "search.h"
#include "licensing.h"
#include "auxprogs.h"
#include "network.h"
#include "passwords.h"
#include "mirror.h"

#ifndef SIGTSTP
#define SIGTSTP 20
#endif

#define OLDMOUSE 1

void ask_refresh (char *fn);

static int do_key_viewlocal (int key, int action);
static int do_key_viewremote (int key, int action);
static int do_key_CC (int key, int action);
static int do_key_anymode (int key, int action);

void do_walk (int key);
void do_mark (int key);
void do_mark_by_mask (int key);
void do_mark_multiple (int key);
void do_enter_dir (int key);
void do_transfer (int where);
void do_upload_marked (void);
void do_download_marked (void);
void do_local_delete (int key);
void do_remote_delete (int key);

int delete_remote_file (char *dirname, char *filename, int *noisy);
int delete_remote_subdir (char *dirname, char *filename,
                          int *noisy_notempty, int *noisy_readonly);

static void make_transfer (int direction);
static void make_transfer_marked (int direction);

static void make_download_everything (void);
static void make_download_byname (void);
static void make_get_marked_total (int keep_tree);
static void make_get_file_byname (char *name, char *newname);
static void make_launch_wget (void);
static void make_export (void);

static void make_remote_mkdir (void);
static void make_remote_delete (void);
static void make_remote_select (int walkonly, int movepointer);
static void make_view_remote (int browser);
static void make_remote_chmod (void);

static void make_select_language (void);
static void make_register (void);

static void drawline_select_language (int n, int len, int shift, int pointer, int row, int col);
static void drawline_select_font (int n, int len, int shift, int pointer, int row, int col);

int remove_marks (directory *d);

static int  prevmode = VIEW_CONTROL;
static flyfont *F = NULL;
static int nf = 0;

/* ------------------------------------------------------------- */

static int do_key_CC (int key, int action)
{
    switch (action)
    {
    case KEY_HELP:
        if (!site.set_up) help (M_HLP_SHORT);
        else              help (M_HLP_CC);
        break;

    case KEY_UP:
        if (cchistory.fline == -1)
            cchistory.fline = max1 (cchistory.n - (video_vsize()-2), 0);
        cchistory.fline = max1 (0, cchistory.fline-1);
        break;

    case KEY_DOWN:
        if (cchistory.fline == -1)
            cchistory.fline = max1 (cchistory.n - (video_vsize()-2), 0);
        cchistory.fline = min1 (cchistory.n-1, cchistory.fline+1);
        break;

    case KEY_PGUP:
        if (cchistory.fline == -1)
            cchistory.fline = max1 (cchistory.n - (video_vsize()-2), 0);
        cchistory.fline = max1 (cchistory.fline-(video_vsize()-2), 0);
        break;

    case KEY_PGDN:
        if (cchistory.fline == -1) cchistory.fline =
            max1 (cchistory.n - (video_vsize()-2), 0);
        cchistory.fline = min1 (cchistory.fline+(video_vsize()-2),
                             max1 (cchistory.n - (video_vsize()-2), 0));
        break;

    case KEY_LEFT:
        if (cchistory.shift > 0) cchistory.shift--;
        break;

    case KEY_PG_LEFT:
        cchistory.shift = max1 (0, cchistory.shift-10);
        break;

    case KEY_RIGHT:
        cchistory.shift++;
        break;

    case KEY_PG_RIGHT:
        cchistory.shift += 10;
        break;

    case KEY_HOME:
        cchistory.fline = 0;
        cchistory.shift = 0;
        break;

    case KEY_END:
    case KEY_ESC:
        cchistory.fline = -1;
        cchistory.shift = 0;
        break;

    default:
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------- */

static int do_key_viewremote (int key, int action)
{

    int     i, rsp, rc, f;
    char    buffer[2048];

    if (!site.set_up)
    {
        if (action == KEY_HELP)
            help (M_HLP_SHORT);
        return 0;
    }

    switch (action)
    {
    case KEY_HELP:
        help (M_HLP_DIR_RAW);
        break;

    case KEY_FM_VIEW_INT:
        if (!(RCURFILE.flags & FILE_FLAG_REGULAR)) break;
        make_view_remote (BROWSER_INTERNAL);
        break;

    case KEY_FM_VIEW_EXT:
        if (!(RCURFILE.flags & FILE_FLAG_REGULAR)) break;
        make_view_remote (BROWSER_EXTERNAL);
        break;

    case KEY_FM_DOWNLOAD:
        make_transfer (DOWN);
        break;

    case KEY_FM_COPY:
        do_transfer (DOWN);
        break;

    case KEY_FM_DOWNLOAD_EVERYTHING:
        make_download_everything ();
        break;

    case KEY_FM_DOWNLOAD_WGET:
        make_launch_wget ();
        break;

    case KEY_EXPORT_MARKED:
        make_export ();
        break;

    case KEY_FM_MKDIR:
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            return ERR_FATAL;
        }
        make_remote_mkdir ();
        break;

    case KEY_FM_SAVE_LISTING:
        buffer[0] = '\0';
        if (entryfield (MSG(M_ENTER_LISTING_FILENAME), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
        save_listing (buffer);
        break;

    case KEY_FM_RENAME:
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            return ERR_FATAL;
        }
        if (!(RCURFILE.flags & FILE_FLAG_REGULAR ||
              RCURFILE.flags & FILE_FLAG_DIR)) break;
        strcpy (buffer, RCURFILE.name);
        if (entryfield (MSG(M_ENTER_NEWNAME), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
            buffer[0] == 0) break;
        rc = set_wd (RCURDIR.name, TRUE);
        if (rc) return rc;
        SendToServer (TRUE, NULL, NULL, "RNFR %s", RCURFILE.name);
        SendToServer (TRUE, &rsp, NULL, "RNTO %s", buffer);
        if (rsp != 2)
            fly_ask_ok (ASK_WARN, MSG(M_RENAME_FAILED), RCURFILE.name, buffer);
        else
            ask_refresh (buffer);
        display.quicksearch[0] = '\0';
        break;

    case KEY_FM_DELETE:
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            return ERR_FATAL;
        }
        do_remote_delete (action);
        break;

    case KEY_CHANGE_PERMISSIONS:
        if (!site.features.chmod_works)
        {
            fly_ask_ok (0, MSG(M_NO_CHMOD), NULL);
        }
        else
        {
            make_remote_chmod ();
        }
        break;

    case KEY_FM_SEARCH:
        break;

    case KEY_FM_REPEAT_SEARCH:
        break;

    case KEY_FM_SORT_NAME:
        strcpy (buffer, RCURFILE.name);
        site.sortmode = SORT_NAME;
        sort_remote_files (RCURFILE.name);
        break;

    case KEY_FM_SORT_EXT:
        site.sortmode = SORT_EXT;
        sort_remote_files (RCURFILE.name);
        break;

    case KEY_FM_SORT_TIME:
        site.sortmode = SORT_TIME;
        sort_remote_files (RCURFILE.name);
        break;

    case KEY_FM_SORT_SIZE:
        site.sortmode = SORT_SIZE;
        sort_remote_files (RCURFILE.name);
        break;

    case KEY_FM_SORT_UNSORT:
        site.sortmode = SORT_UNSORTED;
        sort_remote_files (RCURFILE.name);
        break;

    case KEY_FM_SORT_REVERSE:
        site.sortdirection = - site.sortdirection;
        sort_remote_files (RCURFILE.name);
        break;

    case KEY_FM_REREAD:
        if (site.ndir == 1) break;
        strcpy (buffer, RCURFILE.name);
        rc = set_wd (RCURDIR.name, TRUE);
        if (rc) break;
        rc = retrieve_dir (RCURDIR.name);
        if (rc) break;
        //set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
        try_set_remote_cursor (buffer);
        break;

    case KEY_FM_LOAD_DESCRIPTIONS:
        if (site.ndir == 1) break;
        if (RCURFILE.flags & FILE_FLAG_DIR &&
            fly_ask_ok (0, MSG (M_CANT_USE_DIR_AS_INDEX_FILE), RCURFILE.name))
            break;
        if (!fly_ask (ASK_YN, MSG (M_LOAD_DESCRIPTIONS), NULL, RCURFILE.name)) break;
        i = RCURFILENO;
        attach_descriptions (i);
        status.load_descriptions = TRUE;
        break;

    case KEY_FM_GO_ROOT:
        put_message (MSG(M_GOING_ROOT));
        change_dir ("/");
        break;

    case KEY_FM_SELECTALL:
        if (options.new_transfer)
            do_mark_multiple (action);
        else
        {
            for (i=0; i<RNFILES; i++)
                if (markable(RFILE(i)))
                    RFILE(i).flags |= FILE_FLAG_MARKED;
        }
        break;
/*
    case KEY_FM_SELECT_ALLDIRS:
        for (i=0; i<site.ndir; i++)
            for (j=0; j<site.dir[i].nfiles; j++)
                if (site.dir[i].files[j].flags & FILE_FLAG_REGULAR)
                    site.dir[i].files[j].flags |= FILE_FLAG_MARKED;
        break;
*/
    case KEY_FM_MASK_SELECT:
        if (options.new_transfer)
            do_mark_by_mask (action);
        else
        {
            strcpy (buffer, "*");
            if (entryfield (MSG(M_ENTER_SELECT_FILTER), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
            for (i=0; i<RNFILES; i++)
            {
                if (markable(RFILE(i)))
                {
                    if (fnmatch1 (buffer, RFILE(i).name, 0) == 0)
                        RFILE(i).flags |= FILE_FLAG_MARKED;
                }
            }
        }
        break;

    case KEY_FM_MASK_DESELECT:
        if (options.new_transfer)
            do_mark_by_mask (action);
        else
        {
            strcpy (buffer, "*");
            if (entryfield (MSG(M_ENTER_SELECT_FILTER), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
            for (i=0; i<RNFILES; i++)
            {
                if ((RFILE(i).flags & FILE_FLAG_MARKED) &&
                    fnmatch1 (buffer, RFILE(i).name, 0) == 0)
                    RFILE(i).flags &= ~FILE_FLAG_MARKED;
            }
        }
        break;

    case KEY_FM_DESELECTALL:
        if (options.new_transfer)
            do_mark_multiple (action);
        else
            for (i=0; i<RNFILES; i++)
            {
                if (RFILE(i).flags & FILE_FLAG_MARKED)
                    RFILE(i).flags &= ~FILE_FLAG_MARKED;
            }
        break;

/*
    case KEY_FM_DESELECT_ALLDIRS:
        for (i=0; i<site.ndir; i++)
            for (j=0; j<site.dir[i].nfiles; j++)
                if (site.dir[i].files[j].flags & FILE_FLAG_MARKED)
                    site.dir[i].files[j].flags &= ~FILE_FLAG_MARKED;
        break;
*/
    case KEY_FM_SELECT:
        if (options.new_transfer)
            do_mark (action);
        else
            make_remote_select (0, 1);
        break;

    case KEY_FM_COMPUTE_DIRSIZE:
        if (options.new_transfer)
            do_walk (action);
        else
            make_remote_select (1, 0);
        break;

    case KEY_FM_INVERT_SEL:
        if (options.new_transfer)
            do_mark_multiple (action);
        else
            for (i=0; i<RNFILES; i++)
            {
                if (markable(RFILE(i)))
                    RFILE(i).flags ^= FILE_FLAG_MARKED;
            }
        break;

    case KEY_REVIEW_MARKED:
        do_review_marked (action);
        break;

    case KEY_UP:
    case KEY_DOWN:
    case KEY_HOME:
    case KEY_END:
    case KEY_PGUP:
    case KEY_PGDN:
        display.quicksearch[0] = '\0';
        fly_scroll_it (key, &(RFIRSTITEM), &(RCURFILENO), RNFILES, p_nlf());
        fix_screen_pos ();
        if (key == KEY_HOME) display.rshift = 0;
        break;

    case KEY_RIGHT:
        display.rshift++;
        break;

    case KEY_PG_RIGHT:
        display.rshift += 10;
        break;

    case KEY_LEFT:
        if (display.rshift) display.rshift--;
        break;

    case KEY_PG_LEFT:
        display.rshift = max1 (0, display.rshift-10);
        break;

    case KEY_FM_CHDIR:
        strcpy (buffer, RCURDIR.name);
        if (entryfield (MSG(M_ENTER_DIRECTORY), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
            buffer[0] == '\0') break;
        put_message (MSG(M_CHANGING_DIR));
        if (options.new_transfer)
            r_chdir (buffer, 0);
        else
            change_dir (buffer);
        display.quicksearch[0] = '\0';
        break;

    case KEY_FM_GO_UP:
        put_message (MSG(M_GOING_UP));
        if (options.new_transfer)
            r_chdir ("..", 0);
        else
            change_dir ("..");
        display.quicksearch[0] = '\0';
        break;

    case KEY_FM_ENTER_DIR:
    case KEY_ENTER:
        if (options.new_transfer)
            do_enter_dir (action);
        else
        {
            if (!(RCURFILE.flags & (FILE_FLAG_DIR|FILE_FLAG_LINK))) break;
            if (site.system_type == SYS_MVS_NATIVE)
            {
                dmsg ("MVS: entering %s from %s\n", RCURFILE.name, site.u.pathname);
                if (strcmp (RCURFILE.name, "..") == 0)
                {
                    // check for going to the "top"
                    if (strchr (site.u.pathname, '.') == NULL)
                    {
                        dmsg ("MVS: at the top\n");
                        buffer[0] = '\0';
                        if (entryfield (MSG(M_ENTER_DIRECTORY), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
                            buffer[0] == 0) break;
                        put_message (MSG(M_CHANGING_DIR));
                        change_dir ("..");
                        change_dir (buffer);
                        break;
                    }
                    else
                    {
                        change_dir (RCURFILE.name);
                        display.quicksearch[0] = '\0';
                    }
                }
                //else if (strchr (RCURFILE.name, '.') != NULL)
                //{
                //    p = strdup (RCURFILE.name);
                //    *(strchr (p, '.')) = '\0';
                //    change_dir (p);
                //    free (p);
                //    display.quicksearch[0] = '\0';
                //    break;
                //}
                else
                {
                    change_dir (RCURFILE.name);
                    display.quicksearch[0] = '\0';
                }
            }
            else
            {
                change_dir (RCURFILE.name);
                display.quicksearch[0] = '\0';
            }
        }
        break;

    default:
#if OLDMOUSE
        if (IS_MOUSE (key))
        {
            dmsg ("processing mouse: ev=%d, x=%d, y=%d; p_slf=%d\n",
                  MOU_EVTYPE (key), MOU_X(key), MOU_Y(key), p_slf());
            if (status.split_view &&
                MOU_EVTYPE (key) != MOUEV_MOVE &&
                MOU_X(key) < video_hsize()/2)
            {
                set_view_mode (VIEW_LOCAL);
                rc = do_key_viewlocal (key, action);
                if (rc == 0) rc = do_key_anymode (key, action);
                return rc;
            }
            //f = fl_opt.has_osmenu ? 0 : 1;
            f = p_slf ();
            switch (MOU_EVTYPE (key))
            {
            case MOUEV_B1SC:
            case MOUEV_B1DN:
            case MOUEV_B2DN:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + RFIRSTITEM;
                if (i > RNFILES-1) break;
                RCURFILENO = i;
                return 1;

            case MOUEV_B1DC:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + RFIRSTITEM;
                if (i > RNFILES-1) break;
                RCURFILENO = i;
                if (!(RCURFILE.flags & (FILE_FLAG_DIR|FILE_FLAG_LINK))) break;
                change_dir (RCURFILE.name);
                display.quicksearch[0] = '\0';
                return 1;

            case MOUEV_B2SC:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + RFIRSTITEM;
                if (i > RNFILES-1) break;
                RCURFILENO = i;
                update (0);
                make_remote_select (0, 0);
                return 1;
            }
        }
#endif
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------- */

static int do_key_viewlocal (int key, int action)
{
    int   i, f, rc;
    char  buffer[1024], buffer2[512], *p;
    long  ln;

    switch (action)
    {
    case KEY_HELP:
        help (M_HLP_LOCAL_PARSED);
        break;

    case KEY_FM_CHDIR:
        if (fl_opt.has_driveletters)
        {
            strcpy (buffer, "A:");
            buffer[0] = local.drive + 'A';
        }
        else
            buffer[0] = '\0';
        strcat (buffer, local.dir.name);
        if (entryfield (MSG(M_ENTER_LOCAL_DIRECTORY), buffer, sizeof(buffer), 0)
            != PRESSED_ESC && buffer[0] != 0)
            set_local_path (buffer);
        build_local_filelist (NULL);
        break;

    case KEY_FM_VIEW_INT:
        if (!(LCURFILE.flags & FILE_FLAG_REGULAR)) break;
        ln = load_file (LCURFILE.name, &p);
        if (ln <= 0) break;
        browse_buffer (p, ln, LCURFILE.name);
        break;

    case KEY_FM_VIEW_EXT:
        if (!(LCURFILE.flags & FILE_FLAG_REGULAR)) break;
        snprintf1 (buffer, sizeof(buffer), "%s %s", options.texteditor, local.dir.name);
        str_cats (buffer, LCURFILE.name);
        fly_launch (buffer, TRUE, FALSE);
        break;

    case KEY_FM_SORT_NAME:
        strcpy (buffer2, LCURFILE.name);
        local.sortmode = SORT_NAME;
        sort_local_files ();
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_SORT_EXT:
        strcpy (buffer2, LCURFILE.name);
        local.sortmode = SORT_EXT;
        sort_local_files ();
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_SORT_TIME:
        strcpy (buffer2, LCURFILE.name);
        local.sortmode = SORT_TIME;
        sort_local_files ();
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_SORT_SIZE:
        strcpy (buffer2, LCURFILE.name);
        local.sortmode = SORT_SIZE;
        sort_local_files ();
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_SORT_UNSORT:
        strcpy (buffer2, LCURFILE.name);
        local.sortmode = SORT_UNSORTED;
        sort_local_files ();
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_SORT_REVERSE:
        strcpy (buffer2, LCURFILE.name);
        local.sortdirection = - local.sortdirection;
        sort_local_files ();
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_UPLOAD:
        if (!site.set_up) break;
        make_transfer (UP);
        break;

    case KEY_FM_COPY:
        do_transfer (UP);
        break;

    case KEY_FM_MKDIR:
        buffer[0] = '\0';
        if (entryfield (MSG(M_MKDIR_ENTER_NAME), buffer, sizeof(buffer), 0) == PRESSED_ESC || buffer[0] == '\0') break;
#ifdef __WIN32__        
        rc = mkdir (buffer);
#else
        rc = mkdir (buffer, 0777);
#endif
        if (rc) fly_ask_ok (ASK_WARN, MSG(M_CANT_CREATE_DIRECTORY), buffer);
        strcpy (buffer2, local.dir.name);
        strcat (buffer2, buffer);
        build_local_filelist (NULL);
        try_set_local_cursor (buffer);
        break;

    case KEY_FM_RENAME:
        if (!(LCURFILE.flags & FILE_FLAG_REGULAR ||
              LCURFILE.flags & FILE_FLAG_DIR)) break;
        strcpy (buffer2, LCURFILE.name);
        if (entryfield (MSG(M_ENTER_NEWNAME), buffer2, sizeof(buffer2), 0) == PRESSED_ESC ||
            buffer2[0] == 0) break;
        if (rename (LCURFILE.name, buffer2))
            fly_ask_ok (ASK_WARN, MSG(M_RENAME_FAILED), LCURFILE.name, buffer2);
        else
        {
            build_local_filelist (NULL);
            try_set_local_cursor (buffer2);
        }
        break;

    case KEY_FM_DELETE:
        do_local_delete (action);
        break;

    case KEY_FM_REREAD:
        strcpy (buffer2, local.dir.name);
        build_local_filelist (NULL);
        try_set_local_cursor (buffer2);
        break;

    case KEY_FM_GO_ROOT:
        chdir ("/");
        build_local_filelist (NULL);
        break;

    case KEY_FM_SELECTALL:
        if (options.new_transfer)
            do_mark_multiple (action);
        else
        {
            for (i=0; i<LNFILES; i++)
                if (markable (LFILE(i)))
                    LFILE(i).flags |= FILE_FLAG_MARKED;
        }
        break;

    case KEY_FM_DESELECTALL:
        if (options.new_transfer)
            do_mark_multiple (action);
        else
        {
            for (i=0; i<LNFILES; i++)
                if (LFILE(i).flags & FILE_FLAG_MARKED)
                    LFILE(i).flags &= ~FILE_FLAG_MARKED;
        }
        break;

    case KEY_FM_MASK_SELECT:
        if (options.new_transfer)
            do_mark_by_mask (action);
        else
        {
            strcpy (buffer, "*");
            if (entryfield (MSG(M_ENTER_SELECT_FILTER), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
            for (i=0; i<LNFILES; i++)
            {
                if (markable(LFILE(i)) && fnmatch1 (buffer, LFILE(i).name, 0) == 0)
                    LFILE(i).flags |= FILE_FLAG_MARKED;
            }
        }
        break;

    case KEY_FM_MASK_DESELECT:
        if (options.new_transfer)
            do_mark_by_mask (action);
        else
        {
            strcpy (buffer, "*");
            if (entryfield (MSG(M_ENTER_SELECT_FILTER), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
            for (i=0; i<LNFILES; i++)
            {
                if ((LFILE(i).flags & FILE_FLAG_MARKED) &&
                    fnmatch1 (buffer, LFILE(i).name, 0) == 0)
                    LFILE(i).flags &= ~FILE_FLAG_MARKED;
            }
        }
        break;

    case KEY_FM_SELECT:
        if (options.new_transfer)
            do_mark (action);
        else
        {
            if (markable (LCURFILE))
                LCURFILE.flags ^= FILE_FLAG_MARKED;
            LCURFILENO = min1 (LNFILES-1, LCURFILENO+1);
            LFIRSTITEM = max1 (LFIRSTITEM, min1 (LFIRSTITEM+1, LCURFILENO-p_nl()+1));
        }
        break;

    case KEY_FM_INVERT_SEL:
        if (options.new_transfer)
            do_mark_multiple (action);
        else
        {
            for (i=0; i<LNFILES; i++)
                if (markable (LFILE(i)))
                    LFILE(i).flags ^= FILE_FLAG_MARKED;
        }
        break;

    case KEY_UP:
    case KEY_DOWN:
    case KEY_HOME:
    case KEY_END:
    case KEY_PGUP:
    case KEY_PGDN:
        display.quicksearch[0] = '\0';
        fly_scroll_it (key, &(LFIRSTITEM), &(LCURFILENO), LNFILES, p_nlf());
        break;

    case KEY_RIGHT:
        display.lshift++;
        break;

    case KEY_PG_RIGHT:
        display.lshift += 10;
        break;

    case KEY_LEFT:
        if (display.lshift) display.lshift--;
        break;

    case KEY_PG_LEFT:
        display.lshift = max1 (0, display.lshift-10);
        break;

    case KEY_FM_GO_UP:
        strcpy (buffer2, local.dir.name);
        chdir ("..");
        build_local_filelist (buffer2);
        break;

    case KEY_ENTER:
    case KEY_FM_ENTER_DIR:
        if (LCURFILE.flags & FILE_FLAG_DIR)
        {
            strcpy (buffer2, local.dir.name);
            chdir (LCURFILE.name);
            build_local_filelist (buffer2);
            display.quicksearch[0] = '\0';
        }
        else
        {
            if ((LCURFILE.flags & FILE_FLAG_REGULAR) &&
                (LCURFILE.rights & (S_IXUSR|S_IXGRP|S_IXOTH)))
            {
                buffer[0] = '\0';
                if (fl_opt.has_driveletters)
                {
                    buffer[0] = query_drive () + 'a';
                    buffer[1] = ':';
                    buffer[2] = '\0';
                }
                strcat (buffer, local.dir.name);
                str_cats (buffer, LCURFILE.name);
                strcat (buffer, " ");
                if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
                fly_launch (buffer, TRUE, TRUE);
            }
        }
        break;

    default:
#if OLDMOUSE
        if (IS_MOUSE (key))
        {
            if (status.split_view && MOU_EVTYPE (key) != MOUEV_MOVE && MOU_X(key) >= video_hsize()/2)
            {
                set_view_mode (VIEW_REMOTE);
                rc = do_key_viewremote (key, action);
                if (rc == 0) rc = do_key_anymode (key, action);
                return rc;
            }
            f = fl_opt.has_osmenu ? 0 : 1;
            switch (MOU_EVTYPE (key))
            {
            case MOUEV_B1SC:
            case MOUEV_B1DN:
            case MOUEV_B2DN:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + LFIRSTITEM;
                if (i > LNFILES-1) break;
                LCURFILENO = i;
                return 1;

            case MOUEV_B1DC:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + LFIRSTITEM;
                if (i > LNFILES-1) break;
                LCURFILENO = i;
                if (LCURFILE.flags & FILE_FLAG_DIR)
                {
                    strcpy (buffer2, local.dir.name);
                    chdir (LCURFILE.name);
                    build_local_filelist (buffer2);
                }
                if ((LCURFILE.flags & FILE_FLAG_REGULAR) &&
                    (LCURFILE.rights & (S_IXUSR|S_IXGRP|S_IXOTH)))
                {
                    buffer[0] = '\0';
                    if (fl_opt.has_driveletters)
                    {
                        buffer[0] = query_drive () + 'a';
                        buffer[1] = ':';
                        buffer[2] = '\0';
                    }
                    strcat (buffer, local.dir.name);
                    str_cats (buffer, LCURFILE.name);
                    strcat (buffer, " ");
                    if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
                    fly_launch (buffer, TRUE, TRUE);
                }
                display.quicksearch[0] = '\0';
                return 1;

            case MOUEV_B2SC:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + LFIRSTITEM;
                if (i > LNFILES-1) break;
                LCURFILENO = i;
                if (markable(LCURFILE)) LCURFILE.flags ^= FILE_FLAG_MARKED;
                return 1;
            }
        }
#endif
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------- */

static int do_key_anymode (int key, int action)
{
    char buffer[4096], *url, *p;
    int  i, a, newfont, startpos, rc, rsp;
    int  marked_no, marked_dirs;
    int64_t marked_size;
    url_t  u, u1;
    struct hostent     *remote;
    unsigned long      addr;

    switch (action)
    {
    case KEY_MAINHELP:
        help (M_HLP_MAIN);
        break;

    case KEY_FM_CHLOCAL_DRIVE:
        if (!fl_opt.has_driveletters)
        {
            fly_ask_ok (0, MSG(M_UNSUPPORTED), fl_opt.platform_name);
            break;
        }
        choose_local_drive ();
        build_local_filelist (NULL);
        display.view_mode = VIEW_LOCAL;
        break;

    case KEY_SHORTHELP:
        help (M_HLP_SHORT);
        break;

    case KEY_USING_MENUS:
        help (M_HLP_USING_MENU);
        break;

    case KEY_CRASH:
        *((char *)NULL) = 0;
        break;

    case KEY_GEN_CHANGE_TRANSFER_MODE:
        if (!site.set_up) break;
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            break;
        }
        status.binary = !status.binary;
        SetTransferMode ();
        update_switches ();
        break;

    case KEY_FM_DOWNLOAD_BYNAME:
        if (!site.set_up) break;
        make_download_byname ();
        break;

    case KEY_SYNC_BY_DOWNLOAD:
        if (!site.set_up) break;
        if (!fly_ask (ASK_YN, MSG(M_SYNC1), NULL,
                      RCURDIR.name, local.dir.name)) break;
        mirror (LOCAL_WITH_REMOTE, TRUE);
        break;

    case KEY_SYNC_BY_UPLOAD:
        if (!site.set_up) break;
        if (!fly_ask (ASK_YN, MSG(M_SYNC2),
                      NULL, local.dir.name, RCURDIR.name)) break;
        rc = mirror (REMOTE_WITH_LOCAL, TRUE);
        if (rc == 1) fly_ask_ok (0, MSG(M_SYNC3));
        break;

    case KEY_GEN_SAVEMARK:
        if (!site.set_up) break;
        u1 = site.u;
        strcpy (u1.pathname, RCURDIR.name);
        bookmark_add (&u1);
        break;

    case KEY_GEN_EXIT:
        if (!is_http_proxy && site.connected)
        {
            // we're connected
            if (options.ask_logoff)
            {
                // ask about logoff if not disabled
                if (ask_logoff()) break;
            }
            else
            {
                // logoff confirmation disabled, if exit confirmation is enabled, do it
                if (options.ask_exit)
                    if (!fly_ask (ASK_YN, MSG(M_REALLY_EXIT), NULL)) break;
            }
        }
        else
        {
            if (options.ask_exit && !fly_ask (ASK_YN, MSG(M_REALLY_EXIT), NULL)) break;
        }
        Logoff ();
        terminate ();
        exit (0);
        break;

    case KEY_EDIT_PASSWORDS:
        psw_edit ();
        break;

    case KEY_CHANGE_KEYPHRASE:
        psw_chkey ();
        break;

    case KEY_GEN_EDIT_INI:
        strcpy (buffer, options.texteditor);
        strcat (buffer, " ");
        if (strchr (paths.user_libpath, ' ') != NULL) strcat (buffer, "\"");
        strcat (buffer, paths.user_libpath);
        str_cats (buffer, "nftp.ini");
        if (strchr (paths.user_libpath, ' ') != NULL) strcat (buffer, "\"");
        fly_launch (buffer, TRUE, FALSE);
        strcpy (buffer, paths.user_libpath);
        str_cats (buffer, "nftp.ini");
        GetProfileOptions (buffer);
        break;

    case KEY_MENU:
        if (!fl_opt.has_osmenu)
        {
            a = menu_process_line (main_menu, options.menu_open_sub,
                                   options.menu_open_sub != -1);
            if (a > 0) do_key (FMSG_BASE_MENU + a);
        }
        break;

    case KEY_SELECT_FONT:
        if (fl_opt.is_x11)
            put_message (MSG(M_ENUMERATING_FONTS));
        if (F == NULL || nf <= 0)
            nf = fly_get_fontlist (&F, TRUE);
        if (nf <= 0)
        {
            fly_ask_ok (0, MSG(M_UNSUPPORTED), fl_opt.platform_name);
            break;
        }
        // look for current font
        startpos = 0;
        p = fly_get_font ();
        for (i=0; i<nf; i++)
            if (strcmp (F[i].name, p) == 0) startpos = i;
        free (p);
        newfont = fly_choose (MSG(M_SELECT_FONT), 0, &nf, startpos, drawline_select_font, NULL);
        if (newfont >= 0)
        {
            fly_set_font (F[newfont].name);
            video_update (1);
        }
        break;

    case KEY_GEN_LOGOFF:
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            break;
        }
        if (ask_logoff()) break;
        Logoff ();
        break;

    case KEY_GEN_OPEN_URL:
        buffer[0] = '\0';
        if (options.autopaste && (p = get_clipboard ()) != NULL)
        {
            str_scopy (buffer, p);
            free (p);
            // now check whether it really looks like URL
            str_strip2 (buffer, " ");
            if (strchr (buffer, ' ') != NULL) buffer[0] = '\0';
            if (strchr (buffer, '\n') != NULL) buffer[0] = '\0';
            if (strchr (buffer, '\r') != NULL) buffer[0] = '\0';
        }
        if (entryfield (MSG(M_ENTER_HOSTNAME), buffer, sizeof(buffer), 0) == PRESSED_ESC) break;
        if (buffer[0] == '\0') break;
        parse_url (buffer, &u);
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        //if (site.connected) set_view_mode (VIEW_REMOTE);
        break;

    case KEY_GEN_BOOKMARKS:
        init_url (&u);
        if (site.set_up)
            strcpy (u.hostname, site.u.hostname);
        if (!bookmark_select (&u)) break;
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        //if (entryfield (MSG(M_SELECT_DIRECTORY), u.pathname, sizeof(u.pathname), 0) == PRESSED_ESC) break;
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        //if (site.connected) set_view_mode (VIEW_REMOTE);
        break;

    case KEY_GEN_HISTORY:
        strcpy (u.hostname, site.u.hostname);
        if (history_select (&u) < 0) break;
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        break;

    case KEY_GEN_LOGIN:
        init_url (&u);
        if (site.set_up) u = site.u;
        u.userid[0] = '\0';
        u.password[0] = '\0';
        u.pathname[0] = '\0';
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        //if (site.connected) set_view_mode (VIEW_REMOTE);
        break;

    case KEY_UPDATE_NFTP:
        init_url (&u);
        strcpy (u.hostname, "ftp.ayukov.com");
        strcpy (u.pathname, "/pub/nftp");
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        //if (site.connected) set_view_mode (VIEW_REMOTE);
        break;

    case KEY_REGISTER:
        make_register ();
        break;

    case KEY_GEN_SCREENREDRAW:
        video_update (1);
        break;

    case KEY_INTERNAL_VERSION:
        fly_ask_ok (0,
                    "NFTP: New File Transfer Protocol Client\n"
                    "Version%scompiled %s, %s\n"
                    "Platform: %s\n"
                    "Copyright (C) Sergey Ayukov <asv@ayukov.com> 1994-2003\n"
                    "\n"
                    "Registered to: %s\n"
                    "License version: %d",
                    NFTP_VERSION, __DATE__, __TIME__, fl_opt.platform_name,
                    License.status == LIC_NO ? "" : License.name,
                    License.status == LIC_NO ? 0 : License.version
                   );
        break;

    case KEY_GEN_SWITCH_PROXY:
        if (!status.use_proxy && options.firewall_type == 0)
        {
            fly_ask_ok (0, MSG(M_NO_FIREWALLING));
            break;
        }
        if (!is_http_proxy && site.connected)
        {
            if (ask_logoff()) break;
            CloseControlConnection ();
        }
        if (status.use_proxy && site.set_up)
        {
            PutLineIntoResp2 (MSG(M_RESP_LOOKING_UP), site.u.hostname);
            if (strspn (site.u.hostname, " .0123456789") == strlen (site.u.hostname))
            {
                addr = inet_addr (site.u.hostname);
                if (options.dns_lookup)
                    remote = gethostbyaddr ((char *)&addr, 4, AF_INET);
                else
                    remote = NULL;
                if (remote == NULL)
                {
                    PutLineIntoResp2 (MSG(M_RESP_TRYING_IP), site.u.hostname);
                    site.u.ip = addr;
                }
            }
            else
            {
                remote = gethostbyname (site.u.hostname);
                if (remote == NULL)
                {
                    PutLineIntoResp2 (MSG(M_RESP_CANNOT_RESOLVE), site.u.hostname);
                    break;
                }
            }
            if (remote != NULL)
            {
                PutLineIntoResp2 (MSG(M_RESP_FOUND), remote->h_name);
                site.u.ip = *((unsigned long *)(remote->h_addr));
            }
        }
        status.use_proxy = !status.use_proxy;
        adjust_menu_status ();
        break;

    case KEY_TRAFFIC_LIMIT:
        status.traffic_shaping = !status.traffic_shaping;
        adjust_menu_status ();
        break;

    case KEY_PASSIVE_MODE:
        status.passive_mode = !status.passive_mode;
        adjust_menu_status ();
        break;

    case KEY_SYNC_DELETE:
        status.sync_delete = !status.sync_delete;
        adjust_menu_status ();
        break;

    case KEY_PAUSE_TRANSFER:
        status.transfer_paused = !status.transfer_paused;
        adjust_menu_status ();
        break;

    case KEY_GEN_NOOP:
        if (!site.set_up) break;
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            break;
        }
        put_message (MSG(M_SENDING_NOOP));
        rc = SendToServer (TRUE, &rsp, NULL, "NOOP");
        update (1);
        if (rc == 0 && rsp != 2) SendToServer (TRUE, &rsp, NULL, "NOOP");
        if (site.connected)
            fly_ask_ok (0, MSG(M_CONNECTION_OK));
        break;

    case KEY_GEN_INFORMATION:
        if (!site.set_up) break;
        gather_marked (0, &marked_no, &marked_dirs, &marked_size);
        u1 = site.u;
        strcpy (u1.pathname, RCURDIR.name);
        url = compose_url (u1, RCURFILE.name, 0, 0);
        if (fl_opt.has_clipboard)
        {
            if (fly_ask (ASK_YN, MSG(M_INFORMATION2), NULL,
                         marked_no, marked_dirs, pretty64(marked_size), url))
                put_clipboard (url);
        }
        else
        {
            fly_ask_ok (0, MSG(M_INFORMATION1),
                        marked_no, marked_dirs, pretty64(marked_size), url);
        }
        free (url);
        break;

    case KEY_GEN_QUOTE:
        if (!site.set_up) break;
        if (is_http_proxy)
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            break;
        }
        *buffer = 0;
        if (entryfield (MSG(M_ENTER_VERBATIM_COMMAND), buffer, sizeof(buffer), 0)
            != PRESSED_ESC && buffer[0] != 0)
        {
            set_view_mode (VIEW_CONTROL);
            put_message (MSG(M_SENDING_VERBATIM_COMMAND));
            SendToServer (TRUE, NULL, NULL, buffer);
        }
        break;

    case KEY_FM_CHDIR:
        if (!site.set_up) break;
        strcpy (buffer, RCURDIR.name);
        if (entryfield (MSG(M_ENTER_DIRECTORY), buffer, sizeof(buffer), 0) != PRESSED_ESC &&
            buffer[0] != 0)
        {
            put_message (MSG(M_CHANGING_DIR));
            change_dir (buffer);
        }
        break;

    case KEY_GEN_USEFLAGS:
        status.use_flags = !status.use_flags;
        adjust_menu_status ();
        break;

    case KEY_RESOLVE_SYMLINKS:
        status.resolve_symlinks = !status.resolve_symlinks;
        if (status.resolve_symlinks && !status.use_flags) status.use_flags = TRUE;
        adjust_menu_status ();
        break;

    case KEY_GEN_SPLITSCREEN:
        status.split_view = !status.split_view;
        adjust_menu_status ();
        break;

    case KEY_GEN_SHOWTIMESTAMPS:
        status.show_times = !status.show_times;
        adjust_menu_status ();
        break;

    case KEY_GEN_SHOWMINISTATUS:
        status.show_ministatus = !status.show_ministatus;
        adjust_menu_status ();
        break;

    case KEY_SYNC_SUBDIRS:
        status.sync_subdirs = !status.sync_subdirs;
        adjust_menu_status ();
        break;

    case KEY_GEN_MODE_SWITCH:
        if (!site.set_up) break;
        display.dir_mode = (display.dir_mode == MODE_RAW) ? MODE_PARSED : MODE_RAW;
        break;

    case KEY_GEN_SHOW_DESC:
        status.load_descriptions = !status.load_descriptions;
        if (status.load_descriptions) status.split_view = FALSE;
        adjust_menu_status ();
        update (1);
        if (!site.set_up) break;
        if (status.load_descriptions)
        {
            for (i=0; i<RNFILES; i++)
                if (isindexfile (RFILE(i).name)) break;
            if (i == RNFILES) break;
            if (RFILE(i).size >= options.desc_size_limit*1024 ||
                (site.speed > 0. &&
                 RFILE(i).size/site.speed > options.desc_time_limit))
            {
                a = fly_ask (0, MSG(M_LARGE_INDEX_FILE),
                             MSG(M_DESC_DOWNLOAD_OPTIONS),
                             RFILE(i).name,
                             pretty64 (RFILE(i).size));
                if (a == 3) status.load_descriptions = FALSE;
                if (a == 0 || a == 2 || a == 3) break;
            }
            attach_descriptions (i);
        }
        break;

    case KEY_BACKSPACE:
        if (display.view_mode == VIEW_CONTROL) return 0;
        if (display.quicksearch[0] != '\0')
            display.quicksearch[strlen(display.quicksearch)-1] = '\0';
        break;

    case KEY_ESC:
        if (display.view_mode == VIEW_CONTROL) return 0;
        display.quicksearch[0] = '\0';
        break;

    case KEY_GEN_SWITCH_TO_CC:
        switch (display.view_mode)
        {
        case VIEW_CONTROL:
            if (prevmode == VIEW_CONTROL)
                set_view_mode (VIEW_REMOTE);
            else
                set_view_mode (prevmode);
            break;
        case VIEW_REMOTE:
        case VIEW_LOCAL:
            prevmode = display.view_mode;
            set_view_mode (VIEW_CONTROL);
            break;
        }
        cchistory.fline = -1;
        cchistory.shift = 0;
        display.quicksearch[0] = '\0';
        break;

    case KEY_GEN_SWITCH_LOCREM:
        if (display.view_mode == VIEW_CONTROL) break;
        set_view_mode (display.view_mode == VIEW_LOCAL ? VIEW_REMOTE : VIEW_LOCAL);
        break;

    case KEY_FTPS_DEFINE_SERVERS:
        ftpsearch_servers ();
        break;

    case KEY_FTPS_FIND:
        init_url (&u);
        if (site.set_up && display.view_mode == VIEW_REMOTE &&
            RCURFILE.flags & FILE_FLAG_REGULAR)
            p = RCURFILE.name;
        else
            p = "";
        if (ftpsearch_find (p, &u) < 0) break;
        if (Edit_Site (&u, FALSE) != PRESSED_ENTER) break;
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        //if (site.connected) set_view_mode (VIEW_REMOTE);
        break;

    case KEY_FTPS_RECALL:
        init_url (&u);
        if (ftpsearch_recall (&u) < 0) break;
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        if (Login (&u)) break;
        if (options.login_bell) Bell (3);
        //if (site.connected) set_view_mode (VIEW_REMOTE);
        break;

    case KEY_SUSPEND:
        if (!fl_opt.is_unix || fl_opt.is_x11)
        {
            fly_ask_ok (0, MSG(M_UNSUPPORTED), fl_opt.platform_name);
            break;
        }
        raise (SIGTSTP);
        break;

    case KEY_SELECT_LANGUAGE:
        make_select_language ();
        break;

    case KEY_GEN_EXPAND_RIGHT:
        video_set_window_size (video_vsize(), video_hsize()+1);
        break;

    case KEY_GEN_CONTRACT_LEFT:
        video_set_window_size (video_vsize(), video_hsize()-1);
        break;

    case KEY_GEN_EXPAND_DOWN:
        video_set_window_size (video_vsize()+1, video_hsize());
        break;

    case KEY_GEN_CONTRACT_UP:
        video_set_window_size (video_vsize()-1, video_hsize());
        break;

    default:
#if OLDMOUSE
#else
        /* check mouse */
        if (IS_MOUSE (key) && (display.view_mode == VIEW_LOCAL || display.view_mode == VIEW_REMOTE))
        {
            /* first we set cursor to where click occured. we react to
             clicks only */
            ev = MOU_EVTYPE (key);
            if (ev == MOUEV_SC1 || ev == MOUEV_DC1 || ev == MOUEV_DN1 ||
                ev == MOUEV_SC2 || ev == MOUEV_DC2 || ev == MOUEV_DN2)
            {
                if (status.split_view)
                {
                    /* two panels */
                    if (MOU_X(key) < video_hsize()/2)
                    {
                        set_view_mode (VIEW_LOCAL);
                        rc = do_key_viewlocal (key, action);
                        if (rc == 0) rc = do_key_anymode (key, action);
                        return rc;
                    }
                }
                else
                {
                    /* not a split view */
                }
            }
            //f = fl_opt.has_osmenu ? 0 : 1;
            f = p_slf ();
            switch (MOU_EVTYPE (key))
            {
            case MOUEV_B1SC:
            case MOUEV_B1DN:
            case MOUEV_B2DN:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + RFIRSTITEM;
                if (i > RNFILES-1) break;
                RCURFILENO = i;
                return 1;

            case MOUEV_B1DC:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + RFIRSTITEM;
                if (i > RNFILES-1) break;
                RCURFILENO = i;
                if (!(RCURFILE.flags & (FILE_FLAG_DIR|FILE_FLAG_LINK))) break;
                change_dir (RCURFILE.name);
                display.quicksearch[0] = '\0';
                return 1;

            case MOUEV_B2SC:
                if (MOU_Y(key) < f || MOU_Y(key) == video_vsize()-1) break;
                i = (MOU_Y(key)-f) + RFIRSTITEM;
                if (i > RNFILES-1) break;
                RCURFILENO = i;
                update (0);
                make_remote_select (0, 0);
                return 1;
            }
        }
#endif
        a = menu_check_key (main_menu, key);
        if (a > 0)
        {
            do_key (FMSG_BASE_MENU + a);
            break;
        }
        if (display.view_mode == VIEW_CONTROL) return 0;
        if (display.view_mode == VIEW_REMOTE && !site.set_up) return 0;
        // handle quicksearch
        if (!IS_KEY (key)) return 0;
        if (key > 255) break;
        if (strchr ("0123456789abcdefghijklmnopqrstuvwxyz.-_", key) != NULL)
        {
            if (strlen(display.quicksearch) == sizeof(display.quicksearch)-1)
                break;
            buffer[0] = key; buffer[1] = '\0';
            strcat (display.quicksearch, buffer);
            if (display.view_mode == VIEW_REMOTE)
            {
                quicksearch (&RCURDIR);
                fix_screen_pos ();
            }
            if (display.view_mode == VIEW_LOCAL)
            {
                quicksearch (&local.dir);
                local.dir.first_item = max1 (local.dir.current_file-p_nl()/2, 0);
            }
        }
        return 0;
    }

    return 1;
}

/* ------------------------------------------------------------- */

void do_key (int key)
{
    int rc = 0, act = KEY_NOTHING;

    if (key == 0) return;

    if (IS_KEY (key))
    {
        act = options.keytable[key];
        if (act == KEY_NOTHING)
            act = options.keytable2[key];
        dmsg ("assigned action %d to key %d\n", act, key);
    }

    if (IS_SYSTEM (key))
    {
        switch (SYS_TYPE(key))
        {
        case SYSTEM_RESIZE:
            act = KEY_NOTHING;          break;
        case SYSTEM_QUIT:
            act = KEY_GEN_EXIT;         break;
        default:
            act = KEY_NOTHING;
        }
    }

    if (IS_MENU (key)) act = key - FMSG_BASE_MENU;

    switch (display.view_mode)
    {
    case VIEW_REMOTE:  rc = do_key_viewremote (key, act); break;
    case VIEW_CONTROL: rc = do_key_CC (key, act); break;
    case VIEW_LOCAL:   rc = do_key_viewlocal (key, act); break;
    }

    if (rc == 0) rc = do_key_anymode (key, act);

    update (1);
}

/* -------------------------------------------------------------
 actions can be of the following groups:
                        VIEW_CONTROL   VIEW_REMOTE   VIEW_LOCAL VIEW_TRANS
 -- control conn              +            -               -        -
 -- filelist remote           -            +               +        -
 -- filelist local            -            -               -        -
 -- transfer                  -            -               -        +

 new model: at the start, everything is disabled. Then we enable those which
 are always active, then those imposed by active mode (cc, remote, local).
 Then we disable corresponding actions if no connection has been made yet,
 OS is Unix etc.
*/

static int actions_all[] =
{
    KEY_HELP, KEY_MAINHELP, KEY_SHORTHELP, KEY_PG_RIGHT, KEY_PG_LEFT,
    KEY_INTERNAL_VERSION, KEY_USING_MENUS, KEY_MENU, KEY_CRASH, KEY_SUSPEND,

    KEY_GEN_BOOKMARKS, KEY_GEN_LOGIN,
    KEY_GEN_LOGOFF, KEY_GEN_EXIT, KEY_GEN_INFORMATION, KEY_GEN_EDIT_INI,
    KEY_GEN_HISTORY, KEY_GEN_OPEN_URL, KEY_GEN_INFORMATION, KEY_SYNC_DELETE,

    KEY_GEN_CHANGE_TRANSFER_MODE, KEY_GEN_NOOP, KEY_GEN_USEFLAGS,
    KEY_GEN_QUOTE, KEY_GEN_SWITCH_PROXY, KEY_PASSIVE_MODE, KEY_EDIT_PASSWORDS,

    KEY_GEN_SWITCH_TO_CC, KEY_GEN_SWITCH_LOCREM, KEY_GEN_MODE_SWITCH,
    KEY_GEN_SHOW_DESC, KEY_GEN_SAVEMARK, KEY_GEN_SCREENREDRAW,
    KEY_GEN_SPLITSCREEN, KEY_GEN_SHOWTIMESTAMPS, KEY_GEN_SHOWMINISTATUS,

    KEY_FM_DOWNLOAD, KEY_FM_UPLOAD, KEY_FM_DOWNLOAD_EVERYTHING,
    KEY_FM_UPLOAD_EVERYTHING, KEY_FM_DOWNLOAD_BYNAME, KEY_FM_SAVE_LISTING,
    KEY_FM_DOWNLOAD_WGET, KEY_SKIP_TRANSFER, KEY_STOP_TRANSFER,
    KEY_RESTART_TRANSFER, KEY_PAUSE_TRANSFER, KEY_EXPORT_MARKED,

    KEY_FM_VIEW_INT, KEY_FM_VIEW_EXT, KEY_FM_DELETE, KEY_FM_MKDIR,
    KEY_FM_RENAME, KEY_FM_SEARCH, KEY_FM_REPEAT_SEARCH, KEY_FM_COMPUTE_DIRSIZE,
    KEY_CHANGE_PERMISSIONS,

    KEY_FM_SORT_NAME, KEY_FM_SORT_EXT, KEY_FM_SORT_TIME, KEY_FM_SORT_SIZE,
    KEY_FM_SORT_UNSORT, KEY_FM_SORT_REVERSE,

    KEY_SYNC_BY_DOWNLOAD, KEY_SYNC_BY_UPLOAD,

    KEY_FM_REREAD, KEY_FM_ENTER_DIR, KEY_FM_GO_ROOT, KEY_FM_GO_UP,
    KEY_FM_CHDIR, KEY_FM_CHLOCAL_DRIVE, KEY_FM_LOAD_DESCRIPTIONS,
    KEY_FM_COPY, KEY_FM_MOVE,

    KEY_FM_SELECTALL, KEY_FM_DESELECTALL, KEY_FM_SELECT, KEY_FM_MASK_SELECT,
    KEY_FM_MASK_DESELECT, KEY_FM_INVERT_SEL, /*KEY_FM_SELECT_ALLDIRS,
    KEY_FM_DESELECT_ALLDIRS,*/

    KEY_UPDATE_NFTP, KEY_SELECT_LANGUAGE, KEY_REGISTER, KEY_SELECT_FONT,
    KEY_GEN_EXPAND_RIGHT, KEY_GEN_CONTRACT_LEFT, KEY_GEN_EXPAND_DOWN,
    KEY_GEN_CONTRACT_UP, KEY_TRAFFIC_LIMIT,

    KEY_FTPS_FIND, KEY_FTPS_DEFINE_SERVERS, KEY_FTPS_RECALL,
};

static int actions_always[] =
{
    KEY_HELP, KEY_MAINHELP, KEY_SHORTHELP, KEY_PG_RIGHT, KEY_PG_LEFT,
    KEY_INTERNAL_VERSION, KEY_USING_MENUS, KEY_MENU, KEY_CRASH, KEY_SUSPEND,

    KEY_GEN_BOOKMARKS, KEY_GEN_LOGIN,
    KEY_GEN_EXIT, KEY_GEN_EDIT_INI, KEY_GEN_INFORMATION,
    KEY_GEN_HISTORY, KEY_GEN_OPEN_URL, KEY_PASSIVE_MODE,
    KEY_GEN_USEFLAGS, KEY_GEN_SWITCH_PROXY, KEY_EDIT_PASSWORDS,

    KEY_GEN_SWITCH_TO_CC, KEY_GEN_SHOW_DESC, KEY_GEN_SCREENREDRAW,
    KEY_TRAFFIC_LIMIT, KEY_PAUSE_TRANSFER, KEY_RESOLVE_SYMLINKS,
    KEY_SYNC_DELETE,

    KEY_UPDATE_NFTP, KEY_SELECT_LANGUAGE, KEY_REGISTER, KEY_SELECT_FONT,
    KEY_GEN_EXPAND_RIGHT, KEY_GEN_CONTRACT_LEFT, KEY_GEN_EXPAND_DOWN,
    KEY_GEN_CONTRACT_UP,

    KEY_FTPS_FIND, KEY_FTPS_DEFINE_SERVERS, KEY_FTPS_RECALL
};

static int actions_setup[] = {
    KEY_GEN_LOGOFF, KEY_GEN_INFORMATION,
    KEY_GEN_CHANGE_TRANSFER_MODE, KEY_GEN_NOOP, KEY_GEN_QUOTE,
    KEY_GEN_SAVEMARK, KEY_CHANGE_PERMISSIONS,
    KEY_SYNC_BY_DOWNLOAD, KEY_SYNC_BY_UPLOAD,
    KEY_FM_DOWNLOAD, KEY_FM_UPLOAD, KEY_FM_DOWNLOAD_EVERYTHING,
    KEY_FM_UPLOAD_EVERYTHING, KEY_FM_DOWNLOAD_BYNAME, KEY_FM_SAVE_LISTING,
    KEY_FM_DOWNLOAD_WGET, KEY_SKIP_TRANSFER, KEY_STOP_TRANSFER,
    KEY_RESTART_TRANSFER, KEY_FM_COPY, KEY_FM_MOVE,};

static int actions_filelist_remote[] = {
    KEY_GEN_LOGOFF, KEY_GEN_CHANGE_TRANSFER_MODE, KEY_GEN_NOOP, KEY_GEN_QUOTE,
    KEY_GEN_SAVEMARK, KEY_CHANGE_PERMISSIONS,
    KEY_FM_DOWNLOAD, KEY_FM_DOWNLOAD_EVERYTHING,
    KEY_FM_DOWNLOAD_BYNAME, KEY_FM_SAVE_LISTING,
    KEY_FM_DOWNLOAD_WGET, KEY_EXPORT_MARKED,
    KEY_GEN_SWITCH_LOCREM, KEY_GEN_MODE_SWITCH,
    KEY_FM_VIEW_INT, KEY_FM_VIEW_EXT, KEY_FM_DELETE, KEY_FM_MKDIR,
    KEY_FM_RENAME, KEY_FM_SEARCH, KEY_FM_REPEAT_SEARCH, KEY_FM_COMPUTE_DIRSIZE,
    KEY_FM_SORT_NAME, KEY_FM_SORT_EXT, KEY_FM_SORT_TIME, KEY_FM_SORT_SIZE,
    KEY_FM_SORT_UNSORT, KEY_FM_SORT_REVERSE,
    KEY_SYNC_BY_DOWNLOAD, KEY_SYNC_BY_UPLOAD,
    KEY_FM_REREAD, KEY_FM_ENTER_DIR, KEY_FM_GO_ROOT, KEY_FM_GO_UP,
    KEY_FM_CHDIR, KEY_FM_CHLOCAL_DRIVE, KEY_FM_LOAD_DESCRIPTIONS,
    KEY_FM_SELECTALL, KEY_FM_DESELECTALL, KEY_FM_SELECT, KEY_FM_MASK_SELECT,
    KEY_FM_MASK_DESELECT, KEY_FM_INVERT_SEL, /*KEY_FM_SELECT_ALLDIRS,*/
    KEY_GEN_SPLITSCREEN, KEY_GEN_SHOWTIMESTAMPS, KEY_GEN_SHOWMINISTATUS,
    KEY_FM_COPY, KEY_FM_MOVE,
    /*KEY_FM_DESELECT_ALLDIRS*/};

static int actions_filelist_local[] = {
    KEY_GEN_LOGOFF, KEY_GEN_CHANGE_TRANSFER_MODE, KEY_GEN_NOOP, KEY_GEN_QUOTE,
    KEY_GEN_SAVEMARK,
    KEY_FM_UPLOAD, KEY_FM_UPLOAD_EVERYTHING,
    KEY_SKIP_TRANSFER, KEY_STOP_TRANSFER,
    KEY_GEN_SWITCH_LOCREM,
    KEY_FM_VIEW_INT, KEY_FM_VIEW_EXT, KEY_FM_DELETE, KEY_FM_MKDIR,
    KEY_FM_RENAME, KEY_FM_SEARCH, KEY_FM_REPEAT_SEARCH, KEY_FM_COMPUTE_DIRSIZE,
    KEY_FM_SORT_NAME, KEY_FM_SORT_EXT, KEY_FM_SORT_TIME, KEY_FM_SORT_SIZE,
    KEY_FM_SORT_UNSORT, KEY_FM_SORT_REVERSE,
    KEY_SYNC_BY_DOWNLOAD, KEY_SYNC_BY_UPLOAD,
    KEY_FM_REREAD, KEY_FM_ENTER_DIR, KEY_FM_GO_ROOT, KEY_FM_GO_UP,
    KEY_FM_CHDIR, KEY_FM_CHLOCAL_DRIVE, KEY_FM_LOAD_DESCRIPTIONS,
    KEY_FM_SELECTALL, KEY_FM_DESELECTALL, KEY_FM_SELECT, KEY_FM_MASK_SELECT,
    KEY_FM_MASK_DESELECT, KEY_FM_INVERT_SEL, /*KEY_FM_SELECT_ALLDIRS,*/
    KEY_GEN_SPLITSCREEN, KEY_GEN_SHOWTIMESTAMPS, KEY_GEN_SHOWMINISTATUS,
    KEY_FM_COPY, KEY_FM_MOVE,
    /*KEY_FM_DESELECT_ALLDIRS*/};

static int actions_control[] = {
    KEY_GEN_QUOTE};

//static int actions_transfer[] = {
//    KEY_TRAFFIC_LIMIT, KEY_SKIP_TRANSFER, KEY_STOP_TRANSFER, KEY_RESTART_TRANSFER,
//    KEY_PAUSE_TRANSFER};

static int actions_not_x11[] = {
    KEY_SUSPEND};

#define set_status(a,s) \
    for(i=0; i<sizeof(a)/sizeof(int); i++) { \
    actions[a[i]] = s; }

void adjust_menu_status (void)
{
    int i, actions[KEY_MAX];

    for (i=0; i<KEY_MAX; i++)
        actions[i] = -1;

    set_status (actions_all, FALSE);
    set_status (actions_always, TRUE);

    switch (display.view_mode)
    {
    case VIEW_CONTROL:
        set_status (actions_control,  TRUE);
        break;

    case VIEW_REMOTE:
        set_status (actions_filelist_remote, TRUE);
        break;

    case VIEW_LOCAL:
        set_status (actions_filelist_local, TRUE);
        break;

    //case VIEW_TRANSFERSTATS:
    //    set_status (actions_all, FALSE);
    //    set_status (actions_transfer, TRUE);
    //    break;
    }

    if (!site.set_up)
        set_status (actions_setup, FALSE);

    if (fl_opt.is_x11)
    {
        set_status (actions_not_x11, FALSE);
    }

    for (i=0; i<KEY_MAX; i++)
    {
        if (actions[i] != -1)
            menu_chstatus (&i, 1, actions[i]);
    }

    update_switches ();
}

/* ------------------------------------------------------------- */

void set_view_mode (int mode)
{
    display.view_mode = mode;
    adjust_menu_status ();
}

/* ------------------------------------------------------------- */

static void make_download_everything (void)
{
    int            keep_tree, oldmode;
    int            marked_no, marked_dirs;
    int64_t        marked_size;

    gather_marked (1, &marked_no, &marked_dirs, &marked_size);
    if (marked_no == 0)
    {
        fly_ask_ok (0, MSG(M_NO_MARKED_FILES));
        return;
    }
    //if (!fly_ask (ASK_YN, MSG(M_REALLY_TRANSFER_FROM_ALLDIRS), NULL,
    //          marked_no, marked_dirs, pretty64(marked_size))) return;
    keep_tree = fly_ask (0, MSG(M_KEEP_TREE_INTACT), MSG(M_KEEP_TREE2),
                        marked_no, marked_dirs, pretty64(marked_size));
    if (!keep_tree) return;

    oldmode = site.batch_mode;
    site.batch_mode = TRUE;
    make_get_marked_total (2-keep_tree);
    site.batch_mode = oldmode;
}

/* ------------------------------------------------------------- */

static void make_remote_delete (void)
{
    int            i, mk, rc, notempty=TRUE, readonly=TRUE;
    char           buffer[2048];

    if (!fly_ask (ASK_YN|ASK_WARN, MSG(M_REALLY_DELETE), NULL)) return;

    rc = set_wd (RCURDIR.name, TRUE);
    if (rc) return;

    // check whether we have marked files
    mk = FALSE;
    for (i=0; i<RNFILES; i++)
        if (markable (RFILE(i)) && RFILE(i).flags & FILE_FLAG_MARKED)
            mk = TRUE;

    rc = 0;
    buffer[0] = '\0';
    if (mk)
    {
        //if (!options.slowlink)
        //{
        //    set_view_mode (VIEW_CONTROL);
        //    update (1);
        //}

        /* first we must delete files (they are in the current directory) */
        for (i=0; i<RNFILES; i++)
        {
            if ((RFILE(i).flags & FILE_FLAG_MARKED) &&
                (RFILE(i).flags & FILE_FLAG_REGULAR))
            {
                rc += SendToServer (TRUE, NULL, NULL, "DELE %s", RFILE(i).name);
            }
        }
        /* now delete directories */
        for (i=0; i<RNFILES; i++)
        {
            if ((RFILE(i).flags & FILE_FLAG_MARKED) &&
                (RFILE(i).flags & FILE_FLAG_DIR))
            {
                rc = delete_remote_subdir (RCURDIR.name, RFILE(i).name, &notempty, &readonly);
            }
        }
    }
    else
    {
        if (RCURFILE.flags & FILE_FLAG_DIR)
        {
            rc = delete_remote_subdir (RCURDIR.name, RCURFILE.name, &notempty, &readonly);
        }
        else
            SendToServer (TRUE, NULL, NULL, "DELE %s", RCURFILE.name);
    }

    //set_view_mode (VIEW_REMOTE);
    ask_refresh (buffer);
}

/* ------------------------------------------------------------- */

static void make_remote_mkdir (void)
{
    int            rsp;
    char           buffer[2048], dirname[2048];

    buffer[0] = '\0';
    if (entryfield (MSG(M_MKDIR_ENTER_NAME), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
        buffer[0] == '\0') return;

    put_message (MSG(M_MKDIR_CREATING));
    if (site.features.unixpaths)
    {
        strcpy (dirname, RCURDIR.name);
        str_cats (dirname, buffer);
    }
    else
    {
        strcpy (dirname, buffer);
    }
    SendToServer (TRUE, &rsp, NULL, "MKD %s", dirname);

    if (rsp != 2)
        fly_ask_ok (ASK_WARN, MSG(M_CANT_CREATE_DIRECTORY), dirname);
    else
        ask_refresh (buffer);
    display.quicksearch[0] = '\0';
}

/* ------------------------------------------------------------- */

static void make_remote_select (int walkonly, int movepointer)
{
    int            i, j, k, walked, rc;
    int            marked_no, marked_dirs;
    int64_t        marked_size;
    char           buffer[2048], buf[2048];

    if (RCURFILE.flags & FILE_FLAG_REGULAR)
    {
        RCURFILE.flags ^= FILE_FLAG_MARKED;
        goto move;
    }

    // directory marking
    if (markable (RCURFILE) != 0 && RCURFILE.flags & FILE_FLAG_DIR)
    {
        strcpy (buffer, RCURDIR.name);
        str_cats (buffer, RCURFILE.name);
        if (!walkonly && (RCURFILE.flags & FILE_FLAG_MARKED))
        {
            // walk over subtree, clear marks
            for (i=0; i<site.ndir; i++)
            {
                if (strncmp (site.dir[i].name, buffer,
                             strlen(buffer)) != 0) continue;
                for (j=0; j<site.dir[i].nfiles; j++)
                    if (site.dir[i].files[j].flags & FILE_FLAG_MARKED)
                        site.dir[i].files[j].flags &= ~FILE_FLAG_MARKED;
            }
            // clear marking on the directory itself
            RCURFILE.flags &= ~FILE_FLAG_MARKED;
        }
        else
        {
            // check whether tree was already buffered
            walked = FALSE;
            // check every dir under RCURDIR.name/RCURFILE.name
            for (i=0; i<site.ndir; i++)
            {
                // remember, buffer is the full name of the dir under cursor
                if (strncmp (site.dir[i].name, buffer, strlen(buffer)) != 0) continue;
                // found at least one subdir
                walked = TRUE;
                for (j=0; j<site.dir[i].nfiles; j++)
                {
                    if (site.dir[i].files[j].flags & FILE_FLAG_DIR && strcmp (site.dir[i].files[j].name, "..") != 0)
                    {
                        // make the name of directory to look for
                        strcpy (buf, site.dir[i].name);
                        str_cats (buf, site.dir[i].files[j].name);
                        // search for directory among the cached
                        for (k=0; k<site.ndir; k++)
                            if (strcmp (site.dir[k].name, buf) == 0)
                                break;
                        if (k == site.ndir) walked = FALSE;
                        if (!walked) break;
                    }
                }
                if (!walked) break;
            }
            // make sure the tree is buffered
            if (!walked)
            {
                if (options.ask_walktree && !fly_ask (ASK_YN, MSG(M_WALK_TREE), NULL, buffer)) return;
                //oldmode = site.batch_mode;
                //site.batch_mode = TRUE;
                //rc = traverse_directory (RCURFILE.name, 0);
                //site.batch_mode = oldmode;
                //if (rc) return;
                rc = cache_subtree (site.cdir, RCURFILENO);
                set_dir_size (site.cdir, RCURFILENO);
                Bell (2);
            }
            else
                set_dir_size (site.cdir, RCURFILENO);
            // walk over subtree, set marks
            if (!walkonly)
            {
                for (i=0; i<site.ndir; i++)
                {
                    if (strncmp (site.dir[i].name, buffer, strlen(buffer)) != 0) continue;
                    for (j=0; j<site.dir[i].nfiles; j++)
                    {
                        if (markable (site.dir[i].files[j]))
                            site.dir[i].files[j].flags |= FILE_FLAG_MARKED;
                    }
                }
                // set marking on the directory itself
                RCURFILE.flags |= FILE_FLAG_MARKED;
                update (0);
                if (!walked)
                {
                    gather_marked (1, &marked_no, &marked_dirs, &marked_size);
                    fly_ask_ok (0, MSG(M_MARKED_STATISTICS),
                                marked_no, marked_dirs, pretty64(marked_size));
                }
            }
        }
    }

move:
    if (movepointer)
    {
        RCURFILENO = min1 (RNFILES-1, RCURFILENO+1);
        fix_screen_pos ();
    }
}

/* ------------------------------------------------------------- */

static void make_download_byname (void)
{
    char           buffer[2048], buf2[2048], *p;
    int            oldmode;

    buffer[0] = '\0';
    if (site.set_up && site.ndir > 1 && strcmp (RCURFILE.name, "..") != 0)
    {
        strcpy (buffer, RCURFILE.name);
    }
    if (entryfield (MSG(M_FILE_TO_DOWNLOAD), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;
    str_translate (buffer, '\\', '/');
    p = strrchr (buffer, '/');
    if (p == NULL) p = buffer;
    else           p++;
    strcpy (buf2, p);
    if (entryfield (MSG(M_ENTER_RETRIEVE_AS), buf2, sizeof(buf2), 0) == PRESSED_ESC
        || buffer[0] == '\0') return;
    oldmode = site.batch_mode;
    site.batch_mode = FALSE;

    make_get_file_byname (buffer, buf2);

    site.batch_mode = oldmode;
}

/* ------------------------------------------------------------- */

static void make_view_remote (int browser)
{
    char           tmpfile[MAX_PATH];
    char           buf[256], *p;
    int            fh, rc1, rc2, rc;
    struct stat    before, after;
    trans_item     T;

    if (RCURFILE.cached == NULL)
    {
        if (RCURFILE.size > 512*1024 &&
            !fly_ask (ASK_YN|ASK_WARN, MSG(M_TOO_BIG_FOR_INTVIEWER), NULL,
                      RCURFILE.name, pretty64(RCURFILE.size))) return;
        rc = cache_file (site.cdir, RCURFILENO);
        //set_view_mode (VIEW_REMOTE);
        if (rc)
        {
            fly_ask_ok (ASK_WARN, MSG(M_TRANSFER_FAILED), RCURFILE.name);
            return;
        }
    }

    if (browser == BROWSER_INTERNAL)
    {
        browse_buffer (RCURFILE.cached, RCURFILE.size, RCURFILE.name);
    }
    else
    {
        tmpnam1 (tmpfile);
        fh = open (tmpfile, O_RDWR|O_CREAT|O_TRUNC|BINMODE, 0600);
        if (fh < 0) return;
        write (fh, RCURFILE.cached, RCURFILE.size);
        close (fh);

        video_clear (0, 0, video_vsize()-1, video_hsize()-1, options.attr_background);
        video_update (0);
        strcpy (buf, options.texteditor);
        strcat (buf, " ");
        strcat (buf, tmpfile);

        rc1 = stat (tmpfile, &before);
        fly_launch (buf, TRUE, FALSE);
        update (1);
        rc2 = stat (tmpfile, &after);

        if (rc1 == 0 && rc2 == 0 && before.st_mtime != after.st_mtime &&
            fly_ask (ASK_YN, MSG(M_UPLOAD_EDITED), NULL))
        {
            str_translate (tmpfile, '\\', '/');
            p = strrchr (tmpfile, '/');
            *p = '\0';

            T.r_dir  = RCURDIR.name;
            T.l_dir  = tmpfile;
            T.r_name = RCURFILE.name;
            T.l_name = p+1;
            T.mtime  = after.st_mtime;
            T.size   = after.st_size;
            T.perm   = after.st_mode;
            T.description = NULL;
            T.f = NULL;
            T.flags = TF_OVERWRITE;

            transfer (UP, &T, 1, TRUE);

            ask_refresh (NULL);
            //set_view_mode (VIEW_REMOTE);
            if (RCURFILE.cached != NULL)
            {
                free (RCURFILE.cached);
                RCURFILE.cached = NULL;
            }
        }
        remove (tmpfile);
    }
}

/* ------------------------------------------------------------- */

static void make_get_file_byname (char *r_filename, char *l_filename)
{
    trans_item  T;

    T.r_dir = RCURDIR.name;
    T.l_dir = local.dir.name;
    T.r_name = r_filename;
    T.l_name = l_filename;
    T.mtime  = time (NULL);
    T.size   = 0;
    T.perm   = 0644;
    T.description = NULL;
    T.f = NULL;
    T.flags = TF_OVERWRITE;

    transfer (DOWN, &T, 1, TRUE);
}

/* ------------------------------------------------------------- */

static void make_get_marked_total (int keep_tree)
{
    int         i, j, l, nt, nta;
    trans_item  *T;
    char        subdir[8192];

    history_add (&site.u);

    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    for (i=0; i<site.ndir; i++)
    {
        // skip files outside current tree
        l = strlen(RCURDIR.name);
        if (keep_tree && strncmp (site.dir[i].name, RCURDIR.name, l) != 0) continue;
        if (RCURDIR.name[l-1] == '/') l--;
        strcpy (subdir, local.dir.name);
        str_cats (subdir, site.dir[i].name+l);
        for (j=0; j<site.dir[i].nfiles; j++)
        {
            if (!(site.dir[i].files[j].flags & FILE_FLAG_DIR) &&
                site.dir[i].files[j].flags & FILE_FLAG_MARKED)
            {
                T[nt].r_dir  = site.dir[i].name;
                T[nt].r_name = site.dir[i].files[j].name;
                if (keep_tree)
                    T[nt].l_dir  = strdup (subdir);
                else
                    T[nt].l_dir   = strdup (local.dir.name);
                T[nt].l_name = site.dir[i].files[j].name;
                T[nt].mtime  = site.dir[i].files[j].mtime;
                T[nt].size   = site.dir[i].files[j].size;
                T[nt].perm   = site.dir[i].files[j].rights;
                T[nt].description = site.dir[i].files[j].desc;
                T[nt].f = &(site.dir[i].files[j]);
                T[nt].flags = 0;
                nt++;
                if (nt == nta)
                {
                    nta *= 2;
                    T = realloc (T, nta * sizeof (trans_item));
                }
            }
        }
    }

    transfer (DOWN, T, nt, TRUE);

    for (j=0; j<nt; j++) free (T[j].l_dir);
    free (T);

    // remove marks on directories
    remove_marks (&(RCURDIR));
}

/* ------------------------------------------------------------- */

int remove_marks (directory *d)
{
    int    i, j, m, have_marked;
    char   subdir[8192];

    // first we scan for marked files
    have_marked = FALSE;
    for (i=0; i<d->nfiles; i++)
    {
        if ((d->files[i].flags & FILE_FLAG_REGULAR) && (d->files[i].flags & FILE_FLAG_MARKED))
        {
            have_marked = TRUE;
            break;
        }
    }

    // second we scan for directories
    for (i=0; i<d->nfiles; i++)
    {
        if (markable (d->files[i]) && d->files[i].flags & FILE_FLAG_DIR)
        {
            strcpy (subdir, d->name);
            str_cats (subdir, d->files[i].name);
            j = find_directory (subdir);
            if (j == -1) continue;
            m = remove_marks (&(site.dir[j]));
            if (m) have_marked = TRUE;
            else   d->files[i].flags &= ~FILE_FLAG_MARKED;
        }
    }
    return have_marked;
}

/* ------------------------------------------------------------- */

int find_directory (char *s)
{
    int i;

    // find the directory in the cache
    for (i=0; i<site.ndir; i++)
    {
        if (strcmp (site.dir[i].name, s) == 0) break;
    }
    if (i == site.ndir) i = -1;

    return i;
}

/* this function removes directories those names start with 'path'
 from the cache. it always returns 0 */
int invalidate_directory (char *path, int subdirs)
{
    int i, j, d;
    char *current, *p;

    dmsg ("invalidating directory %s\n", path);
    current = strdup (site.dir[site.cdir].name);
    for (i=0, j=0; i<site.ndir; i++)
    {
        if (subdirs ? (str_headcmp (site.dir[i].name, path) == 0) :
            (strcmp (site.dir[i].name, path) == 0))
        {
            free (site.dir[i].files);
            continue;
        }
        site.dir[j++] = site.dir[i];
    }
    site.ndir = j;

    /* locate current directory */
    do
    {
        d = find_directory (current);
        if (d >= 0)
        {
            site.cdir = d;
            break;
        }
        p = strrchr (current, '/');
        if (p == NULL)
        {
            dmsg ("oops: cache is empty?");
            site.cdir = 0;
            break;
        }
        *p = '\0';
    }
    while (current[0] != '\0');

    free (current);
    return 0;
}

/* ------------------------------------------------------------- */

void quicksearch (directory *d)
{
    int i, m, bestmatch = 0, bestmatch_no = 0;

    for (i=0; i<d->nfiles; i++)
    {
        m = str_imatch (d->files[i].name, display.quicksearch);
        if (bestmatch < m)
        {
            bestmatch = m;
            bestmatch_no = i;
        }
    }
    d->current_file = bestmatch_no;
    //d->first_item = max (d->current_file-PAGE_STEP/2, 0);
    display.quicksearch[bestmatch] = '\0';
}

/* ------------------------------------------------------------- */

void ask_refresh (char *fn)
{
    int reread = TRUE, n;

    update (1);
    dmsg ("checking whether our sites are on the same network.\n"
          "remote_ip = %lx, local_ip = %lx, masked: %lx, %lx\n",
          site.u.ip, site.l_ip,
          site.u.ip & 0x00FFFFFF, site.l_ip & 0x00FFFFFF);

    // check whether we are on a same network
    if (options.ask_reread &&
        !((site.u.ip & 0x00FFFFFF) == (site.l_ip & 0x00FFFFFF) && !status.use_proxy))
    {
        reread = fly_ask (ASK_YN, MSG(M_REREAD_REMOTE_DIR), NULL, 0);
    }

    if (reread)
    {
        n = RCURDIR.current_file;
        if (set_wd (RCURDIR.name, FALSE) == ERR_OK)
            retrieve_dir (RCURDIR.name);
        if (fn != NULL)
            try_set_remote_cursor (fn);
        else
        {
            RCURDIR.current_file = min1 (n, RNFILES-1);
        }

    }

    set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
}

/* ------------------------------------------------------------- */

void gather_marked (int sub_only, int *marked_no, int *marked_dirs,
                    int64_t *marked_size)
{
    int i, j, flag;

    *marked_no   = 0;
    *marked_dirs = 0;
    *marked_size = 0;

    if (!site.set_up) return;

    for (i=0; i<site.ndir; i++)
    {
        flag = FALSE;
        // if specified, make sure it's a subdirectory of the current one
        if (sub_only && strncmp (site.dir[i].name, RCURDIR.name,
                                 strlen(RCURDIR.name)) != 0) continue;
        for (j=0; j<site.dir[i].nfiles; j++)
        {
            if ((site.dir[i].files[j].flags & FILE_FLAG_REGULAR) &&
                site.dir[i].files[j].flags & FILE_FLAG_MARKED)
            {
                (*marked_no)++;
                *marked_size += site.dir[i].files[j].size;
                flag = TRUE;
            }
        }
        if (flag) (*marked_dirs)++;
    }
}

/* -------------------------------------------------------------
ask_logoff:
asks whether user really want to log off. also checks for marked
files
returns: 1 - do not logoff
         0 - logoff
*/

int ask_logoff (void)
{
    int marked_no, marked_dirs;
    int64_t marked_size;

    if (!site.connected) return 0;
    if (!options.ask_logoff) return 0;
    gather_marked (0, &marked_no, &marked_dirs, &marked_size);

    if (marked_no)
    {
        return !fly_ask (ASK_YN, MSG(M_LOGOFF_MARKED), NULL,
                         marked_no, marked_dirs, pretty64(marked_size));
    }
    else if (options.ask_exit)
    {
        return !fly_ask (ASK_YN, MSG(M_REALLY_LOGOFF), NULL, site.u.hostname);
    }
    else
    {
        return 0;
    }
}

/* ------------------------------------------------------------- */

#define DOWNLOADABLE (FILE_FLAG_REGULAR | FILE_FLAG_LINK)

static void make_transfer (int direction)
{
    int            i, mk, nf, oldmode;
    char           newname[2048], *path, *name;
    file           *ff, *cf;
    directory      *df;
    trans_item     T;

    history_add (&site.u);

    if (direction == DOWN)
    {
        df = &(RCURDIR);
        cf = &(RFILE(RCURFILENO));
    }
    else
    {
        df = &(local.dir);
        cf = &(LFILE(LCURFILENO));
    }
    ff = df->files;
    nf = df->nfiles;

    // check whether we have marked files
    mk = FALSE;
    for (i=0; i<nf; i++)
        if ((ff[i].flags & DOWNLOADABLE) && (ff[i].flags & FILE_FLAG_MARKED))
            mk = TRUE;

    // do transfer
    if (!mk)
    {
        if (!(cf->flags & DOWNLOADABLE)) return;
        strcpy (newname, cf->name);
        if (entryfield (direction == DOWN ?
                        MSG(M_ENTER_RETRIEVE_AS) : MSG(M_ENTER_UPLOAD_AS),
                        newname, sizeof(newname), 0) == PRESSED_ESC ||
            newname[0] == '\0') return;
        str_translate (newname, '\\', '/');
        oldmode = site.batch_mode;
        site.batch_mode = FALSE;

        T.r_dir = RCURDIR.name;
        if (direction == DOWN)
        {
            T.r_name = RCURFILE.name;
            str_pathdcmp (newname, &path, &name);
            if (path == NULL) T.l_dir = local.dir.name;
            else              T.l_dir = path;
            if (name == NULL) T.l_name = cf->name;
            else              T.l_name = name;
            T.mtime  = RCURFILE.mtime;
            T.size   = RCURFILE.size;
            T.perm   = RCURFILE.rights;
            T.description = RCURFILE.desc;
            T.f = &(RCURFILE);
            T.flags = 0;
            if (options.lowercase_names && str_lastspn (T.l_name, "abcdefghijklmnopqrstuvwxyz") == NULL)
                str_lower (T.l_name);
        }
        else
        {
            T.r_name = newname;
            T.l_dir = local.dir.name;
            T.l_name = LCURFILE.name;
            T.mtime  = LCURFILE.mtime;
            T.size   = LCURFILE.size;
            T.perm   = LCURFILE.rights;
            T.description = LCURFILE.desc;
            T.f = &(LCURFILE);
            T.flags = 0;
        }

        transfer (direction, &T, 1, TRUE);

        if (direction == DOWN)
        {
            if (path != NULL) free (path);
            if (name != NULL) free (name);
        }
        site.batch_mode = oldmode;
    }
    else
    {
        if (!fly_ask (ASK_YN, direction == DOWN ?
                      MSG(M_REALLY_TRANSFER_FROM) :
                      MSG(M_REALLY_TRANSFER_TO), NULL)) return;
        oldmode = site.batch_mode;
        site.batch_mode = TRUE;
        make_transfer_marked (direction);
        site.batch_mode = oldmode;
    }

    if (direction == DOWN)
    {
        build_local_filelist (NULL);
    }
    else
        ask_refresh (NULL);

    //set_view_mode (VIEW_REMOTE);
}

/* ------------------------------------------------------------- */

static void make_transfer_marked (int direction)
{
    int          i, nt, nf, nta, flags;
    trans_item   *T;

    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    nf = direction == DOWN ? RNFILES : LNFILES;
    for (i=0; i<nf; i++)
    {
        flags = direction == DOWN ? RFILE(i).flags : LFILE(i).flags;
        if (flags & FILE_FLAG_MARKED)
        {
            if (flags & FILE_FLAG_DIR)
            {
                /* marked directory. walk it recursively adding files
                 to the queue */
                if (direction == DOWN)
                {
                }
                else
                {
                    //add_dir_to_upload_queue (RFILE(i).name, );
                }
            }
            else if (flags & FILE_FLAG_REGULAR)
            {
                /* marked plain file */
                T[nt].r_dir = RCURDIR.name;
                T[nt].l_dir = local.dir.name;
                if (direction == DOWN)
                {
                    T[nt].r_name = RFILE(i).name;
                    T[nt].l_name = strdup (RFILE(i).name);
                    T[nt].mtime  = RFILE(i).mtime;
                    T[nt].size   = RFILE(i).size;
                    T[nt].perm   = RFILE(i).rights;
                    T[nt].description = RFILE(i).desc;
                    T[nt].f = &(RFILE(i));
                    T[nt].flags = 0;
                    if (options.lowercase_names && str_lastspn (T[nt].l_name, "abcdefghijklmnopqrstuvwxyz") == NULL)
                        str_lower (T[nt].l_name);
                }
                else
                {
                    T[nt].r_name = LFILE(i).name;
                    T[nt].l_name = LFILE(i).name;
                    T[nt].mtime  = LFILE(i).mtime;
                    T[nt].size   = LFILE(i).size;
                    T[nt].perm   = LFILE(i).rights;
                    T[nt].description = LFILE(i).desc;
                    T[nt].f = &(LFILE(i));
                    T[nt].flags = 0;
                }
                nt++;
                if (nt == nta)
                {
                    nta *= 2;
                    T = realloc (T, nta * sizeof (trans_item));
                }
            }
        }
    }

    transfer (direction, T, nt, TRUE);

    if (direction == DOWN)
        for (i=0; i<nt; i++) free (T[i].l_name);
    free (T);
}

/* ------------------------------------------------------------- */

static void make_export (void)
{
    int             i, j, marked_no, marked_dirs;
    int64_t         marked_size;
    char            buffer[4096];
    FILE            *urlfile;
    url_t             u1;

    gather_marked (0, &marked_no, &marked_dirs, &marked_size);
    if (marked_no == 0 && (RCURFILE.flags & FILE_FLAG_DIR)) return;

    // ask confirmation
    buffer[0] = '\0';
    if (entryfield (MSG(M_ENTER_LISTNAME), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;

    // write file with URLs for wget
    urlfile = fopen (buffer, "w");
    if (urlfile == NULL)
    {
        fly_ask_ok (ASK_WARN, MSG(M_CANT_OPEN_FOR_WRITING), buffer);
        return;
    }

    if (marked_no == 0)
    {
        u1 = site.u;
        strcpy (u1.pathname, RCURDIR.name);
        fprintf (urlfile, "%s\n", compose_url (u1, RCURFILE.name, 1, 0));
    }
    else
    {
        u1 = site.u;
        for (i=0; i<site.ndir; i++)
        {
            strcpy (u1.pathname, site.dir[i].name);
            for (j=0; j<site.dir[i].nfiles; j++)
            {
                if ((site.dir[i].files[j].flags & FILE_FLAG_REGULAR) &&
                    site.dir[i].files[j].flags & FILE_FLAG_MARKED)
                {
                    fprintf (urlfile, "%s\n", compose_url (u1, site.dir[i].files[j].name, 1, 0));
                }
            }
        }
    }
    fclose (urlfile);
    build_local_filelist (NULL);
}

/* ------------------------------------------------------------- */

static void make_launch_wget (void)
{
    int             i, j, marked_no, marked_dirs;
    int64_t         marked_size;
    char            wget_input[MAX_PATH], buffer[4096];
    FILE            *urlfile;
    url_t           u1;

    gather_marked (0, &marked_no, &marked_dirs, &marked_size);
    if (marked_no == 0 && (RCURFILE.flags & FILE_FLAG_DIR)) return;

    if (!is_anonymous && !options.passworded_wget)
    {
        fly_ask_ok (0, MSG(M_PASSWORDED_WGET));
        return;
    }

    // ask confirmation
    if (marked_no > 0)
    {
        if (!fly_ask (ASK_YN, MSG(M_WGET_DOWNLOAD_MANY), NULL, marked_no,
                      marked_dirs, pretty64(marked_size))) return;
    }
    else
    {
        if (!fly_ask (ASK_YN, MSG(M_WGET_DOWNLOAD_ONE), NULL, RCURFILE.name)) return;
    }

    // write file with URLs for wget
    tmpnam1 (wget_input);
    urlfile = fopen (wget_input, "w");
    if (urlfile == NULL) return;

    if (marked_no == 0)
    {
        u1 = site.u;
        strcpy (u1.pathname, RCURDIR.name);
        fprintf (urlfile, "%s\n", compose_url (u1, RCURFILE.name, 1, 0));
    }
    else
    {
        u1 = site.u;
        for (i=0; i<site.ndir; i++)
        {
            strcpy (u1.pathname, site.dir[i].name);
            for (j=0; j<site.dir[i].nfiles; j++)
            {
                if ((site.dir[i].files[j].flags & FILE_FLAG_REGULAR) &&
                    site.dir[i].files[j].flags & FILE_FLAG_MARKED)
                {
                    fprintf (urlfile, "%s\n", compose_url (u1, site.dir[i].files[j].name, 1, 0));
                }
            }
        }
    }
    fclose (urlfile);
    chmod (wget_input, 0600);

    snprintf1 (buffer, sizeof(buffer), options.wget_command, wget_input);
    if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;
    fly_launch (buffer, FALSE, FALSE);

    // remove marks
    if (marked_no != 0)
        for (i=0; i<site.ndir; i++)
            for (j=0; j<site.dir[i].nfiles; j++)
                site.dir[i].files[j].flags &= ~FILE_FLAG_MARKED;

    build_local_filelist (NULL);
}

/* ------------------------------------------------------------- */

static char *lang_names[] = {
    "Brazilian    by Ivan F. Martinez <ivanfm@os2brasil.com.br>",
    "Bulgarian    by Martin Mavrov <martin.mavrov@bulgaria.com>",
    "Chinese      by Jacky Yang <jackei.yang@msa.hinet.net>",
    "Czech        by Milan Ulej <milan.ulej@npsumava.cz>",
    "Danish       by Kim Poulsen <kpo@daimi.aau.dk>",
    "Dutch        by HH|CE <info@HHCE.nl>",
    "English      by Sergey Ayukov <asv@sai.msu.ru>",
    "Finnish      by Teemu Mannermaa <wicked@etlicon.fi>",
    "French       by Didier Toulouze <dito@club-internet.fr>",
    "German       by Hubert Brentano <hubert.brentano@koeln.netsurf.de>",
    "Hungarian    by Toth Ferenc <etus@alarmix.net>",
    "Italian      by Pantaleo Nacci <drdivago@mclink.it>",
    "Japanese     by Kazuyoshi Shimizu <kshimz@dd.iij4u.or.jp>",
    "Korean       by Jun Ha Lee <smileguy@jayu.sarang.net>",
    "Norwegian    by Kristoffer Viddal <viddal@writeme.com>",
    "Polish       by Piotr Klaban <makler@man.torun.pl>",
    "Russian      by Sergey Ayukov <asv@sai.msu.ru>",
    "Slovak       by Thomas Ulej <thomas@ulej.sk>",
    "Spanish      by Jose Ruiz <jruiz@ctv.es>",
    "Swedish      by Lennart Carlson <beetle@swipnet.se>",
    "Ukrainian    by Igor Krassikov <kiv@post.materials.kiev.ua>"};

static char *langs[] = {
    "brazilian",
    "bulgarian",
    "chinese",
    "czech",
    "danish",
    "dutch",
    "english",
    "finnish",
    "french",
    "german",
    "hungarian",
    "italian",
    "japanese",
    "korean",
    "norwegian",
    "polish",
    "russian",
    "slovak",
    "spanish",
    "swedish",
    "ukrainian"};

static void make_select_language (void)
{
    int    i, n, startpos, selection;
    char   *current_language;

    n = sizeof(langs)/sizeof(langs[0]);

    current_language = cfg_get_string (CONFIG_NFTP, fl_opt.platform_nick, "language");
    startpos = 5;
    for (i=0; i<n; i++)
        if (str_stristr (langs[i], current_language) != NULL) startpos = i;

    selection = fly_choose (MSG(M_SELECT_LANGUAGE), 0, &n, startpos, drawline_select_language, NULL);
    if (selection >= 0)
    {
        cfg_set_string (CONFIG_NFTP, fl_opt.platform_nick, "language", langs[selection]);
        write_config ();
        nls_init (paths.system_libpath, paths.user_libpath);
        if (main_menu != NULL)
            menu_deactivate (main_menu);
        load_menu ();
        config_fly ();
        menu_activate (main_menu);
        adjust_menu_status ();
        video_update (1);
    }
}

/* ------------------------------------------------------------- */

static void drawline_select_language (int n, int len, int shift, int pointer, int row, int col)
{
    int a = pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back;

    video_put_n_cell (' ', a, len, row, col);
    video_put_str_attr (lang_names[n], min1 (strlen (lang_names[n]), len-1), row, col+1, a);
}

/* ------------------------------------------------------------- */

static void drawline_select_font (int n, int len, int shift, int pointer, int row, int col)
{
    int a = pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back;
    char buffer[128];

    video_put_n_cell (' ', a, len, row, col);
    if (fl_opt.platform == PLATFORM_OS2_PM)
        snprintf1 (buffer, sizeof(buffer), MSG(M_FONT_DESC), F[n].cx, F[n].cy, F[n].name);
    else
        snprintf1 (buffer, sizeof(buffer), "%2d x %2d %s", F[n].cx, F[n].cy, F[n].name);
    video_put_str (buffer, min1 (strlen (buffer), len-1), row, col+1);
}

/* ------------------------------------------------------------- */

static void make_register (void)
{
    int        reply;
    entryfield_t reg[3];

    reg[0].banner = MSG(M_REGISTRATION_NAME);
    reg[0].type = EF_STRING;
    str_scopy (reg[0].value.string, cfg_get_string (CONFIG_NFTP, NULL, "registration-name"));
    reg[0].flags = 0;

    reg[1].type = EF_SEPARATOR;

    reg[2].banner = MSG(M_REGISTRATION_CODE);
    reg[2].type = EF_STRING;
    str_scopy (reg[2].value.string, cfg_get_string (CONFIG_NFTP, NULL, "registration-code"));
    reg[2].flags = 0;

    reply = mle2 (3, reg, MSG(M_USETAB2), 0);

    if (reply == PRESSED_ENTER)
    {
        cfg_set_string (CONFIG_NFTP, NULL, "registration-name", reg[0].value.string);
        cfg_set_string (CONFIG_NFTP, NULL, "registration-code", reg[2].value.string);
        if (reg[0].value.string[0] != '\0')
        {
            check_licensing (paths.user_libpath, paths.system_libpath);
            if (License.status == LIC_VERIFIED)
            {
                fly_ask_ok (0, MSG(M_THANKS));
            }
            else
            {
                if (fly_ask (ASK_YN|ASK_WARN, MSG(M_WRONG_REGCODE), NULL))
                {
                    terminate ();
                    exit (0);
                }
            }
        }
        write_config ();
    }
}

/* ------------------------------------------------------------- */

void set_dir_size (int d, int f)
{
    int            i, j;
    int64_t        s = 0;
    char           buffer[1024];

    // already computed?
    if (site.dir[d].files[f].flags & FILE_FLAG_COMPSIZE) return;

    strcpy (buffer, site.dir[d].name);
    str_cats (buffer, site.dir[d].files[f].name);
    dmsg ("looking for %s\n", buffer);
    // find directory
    for (i=0; i<site.ndir; i++)
        if (strcmp (site.dir[i].name, buffer) == 0) break;
    if (i == site.ndir) return;

    for (j=0; j<site.dir[i].nfiles; j++)
    {
        if (strcmp (site.dir[i].files[j].name, "..") == 0) continue;
        if ((site.dir[i].files[j].flags & FILE_FLAG_DIR) &&
            !(site.dir[i].files[j].flags & FILE_FLAG_COMPSIZE))
            set_dir_size (i, j);
        if ((site.dir[i].files[j].flags & FILE_FLAG_REGULAR) ||
            ((site.dir[i].files[j].flags & FILE_FLAG_DIR) &&
             (site.dir[i].files[j].flags & FILE_FLAG_COMPSIZE)))
            s += site.dir[i].files[j].size;
    }

    site.dir[d].files[f].flags |= FILE_FLAG_COMPSIZE;
    site.dir[d].files[f].size = s;
}

/* ------------------------------------------------------------- */

int delete_subdir (char *path, int *noisy)
{
    char               buffer[MAX_PATH];
    DIR                *d;
    struct dirent      *e;
    struct stat        st;
    int                i, rc;

    d = opendir (path);
    if (d == NULL) return -1; // unreadable?
    rc = 0;

    if (!options.slowlink)
        put_message (MSG(M_DELETING), path);

    for (i=0; ; )
    {
        e = readdir (d);
        if (e == NULL) break;
        if (!strcmp (e->d_name, ".") || !strcmp (e->d_name, "..")) continue;
        str_scopy (buffer, path);
        str_cats (buffer, e->d_name);
        if (stat (buffer, &st) < 0) continue;
        if (S_ISDIR(st.st_mode))
        {
            rc += delete_subdir (buffer, noisy);
        }
        else
        {
            rc += unlink (buffer);
        }
    }
    closedir (d);
    rc += rmdir (path);

    return rc;
}

/* ------------------------------------------------------------- */

static void make_remote_chmod (void)
{
    char buffer[1024], rights[80], buffer2[MAX_PATH];
    int  newrights, rsp, mk, i, rc;

    strcpy (buffer2, RCURFILE.name);
    // check for selected files
    mk = 0;
    for (i=0; i<RNFILES; i++)
        if (RFILE(i).flags & FILE_FLAG_MARKED) mk++;

    rights[0] = '-';
    if (mk)
    {
        strcpy (rights+1, "rwxr-xr-x");
        snprintf1 (buffer, sizeof(buffer), MSG(M_ACCESSRIGHTS1), mk);
    }
    else
    {
        strcpy (rights+1, perm_b2t (RCURFILE.rights));
        snprintf1 (buffer, sizeof(buffer), MSG(M_ACCESSRIGHTS2), RCURFILE.name);
    }

    while (TRUE)
    {
        if (entryfield (buffer, rights+1, sizeof(rights)-1, 0) == PRESSED_ESC) return;
        newrights = perm_t2b (rights);
        if (newrights != 0) break;
        if (strcmp (rights, "----------") == 0) break;
        fly_ask_ok (ASK_WARN, MSG(M_WRONG_RIGHTS), rights+1);
        strcpy (rights+1, perm_b2t (RCURFILE.rights));
    }

    rc = set_wd (RCURDIR.name, TRUE);
    if (rc) return;

    if (mk)
    {
        for (i=0; i<RNFILES; i++)
        {
            if (RFILE(i).flags & FILE_FLAG_MARKED)
            {
                rc = SendToServer (TRUE, &rsp, NULL, "SITE CHMOD %o %s", newrights & 0777, RFILE(i).name);
                if (rc == 0 && rsp == 5)
                {
                    fly_ask_ok (0, MSG(M_NO_CHMOD));
                    site.features.chmod_works = FALSE;
                    return;
                }
            }
        }
    }
    else
    {
        rc = SendToServer (TRUE, &rsp, NULL, "SITE CHMOD %o %s", newrights & 0777, RCURFILE.name);
        if (rc == 0 && rsp == 5)
        {
            fly_ask_ok (0, MSG(M_NO_CHMOD));
            site.features.chmod_works = FALSE;
            return;
        }
    }

    ask_refresh (buffer2);
}

/* ----------------------------------------------------------------- */

void do_walk (int key)
{
    int            walked, rc;

    if (display.view_mode == VIEW_LOCAL)
    {
    }
    else if (display.view_mode == VIEW_REMOTE)
    {
        if (!(RCURFILE.flags & FILE_FLAG_DIR)) return;
        if (strcmp (RCURFILE.name, "..") == 0) return;
        walked = is_walked (site.cdir, RCURFILENO);
        // make sure the tree is buffered
        if (!walked)
        {
            //if (options.ask_walktree && !fly_ask (ASK_YN, MSG(M_WALK_TREE), NULL, buffer)) return;
            //oldmode = site.batch_mode;
            //site.batch_mode = TRUE;
            //rc = traverse_directory (RCURFILE.name, RCURFILE.flags & FILE_FLAG_MARKED);
            rc = cache_subtree (site.cdir, RCURFILENO);
            //site.batch_mode = oldmode;
            if (rc) return;
            set_dir_size (site.cdir, RCURFILENO);
            Bell (2);
        }
        else
        {
            set_dir_size (site.cdir, RCURFILENO);
        }
        //if (RCURFILE.flags & FILE_FLAG_MARKED)
        //{
        //    p = str_sjoin (RCURDIR.name, RCURFILE.name);
        //    set_marks (p);
        //    free (p);
        //}
    }
}

/* ----------------------------------------------------------------- */

void do_mark (int key)
{
    char  *p;

    if (display.view_mode == VIEW_LOCAL)
    {
        if (markable (LCURFILE)) LCURFILE.flags ^= FILE_FLAG_MARKED;
        LCURFILENO = min1 (LNFILES-1, LCURFILENO+1);
        LFIRSTITEM = max1 (LFIRSTITEM, min1 (LFIRSTITEM+1, LCURFILENO-p_nlf()+1));
    }
    else if (display.view_mode == VIEW_REMOTE)
    {
        if (markable (RCURFILE))
        {
            RCURFILE.flags ^= FILE_FLAG_MARKED;
            if (RCURFILE.flags & FILE_FLAG_DIR)
            {
                p = str_sjoin (RCURDIR.name, RCURFILE.name);
                if (RCURFILE.flags & FILE_FLAG_MARKED)
                    set_marks (p);
                else
                    unset_marks (p);
                free (p);
            }
        }
        RCURFILENO = min1 (RNFILES-1, RCURFILENO+1);
        RFIRSTITEM = max1 (RFIRSTITEM, min1 (RFIRSTITEM+1, RCURFILENO-p_nlf()+1));
    }
}

/* ------------------------------------------------------------- */

void do_mark_by_mask (int key)
{
    int    i;
    char   buffer[MAX_PATH];

    strcpy (buffer, "*");
    if (entryfield (MSG(M_ENTER_SELECT_FILTER), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;

    if (display.view_mode == VIEW_LOCAL)
    {
        for (i=0; i<LNFILES; i++)
        {
            if (!markable (LFILE(i))) continue;
            if (fnmatch1 (buffer, LFILE(i).name, 0) == 0)
            {
                if (key == KEY_FM_MASK_SELECT)
                    LFILE(i).flags |= FILE_FLAG_MARKED;
                else
                    LFILE(i).flags &= ~FILE_FLAG_MARKED;
            }
        }
    }
    else if (display.view_mode == VIEW_REMOTE)
    {
        for (i=0; i<RNFILES; i++)
        {
            if (!markable (RFILE(i))) continue;
            if (fnmatch1 (buffer, RFILE(i).name, 0) == 0)
            {
                if (key == KEY_FM_MASK_SELECT)
                    RFILE(i).flags |= FILE_FLAG_MARKED;
                else
                    RFILE(i).flags &= ~FILE_FLAG_MARKED;
            }
        }
    }
}

/* ------------------------------------------------------------- */

void do_mark_multiple (int key)
{
    int    i;

    if (display.view_mode == VIEW_LOCAL)
    {
        for (i=0; i<LNFILES; i++)
        {
            if (!markable (LFILE(i))) continue;
            switch (key)
            {
            case KEY_FM_SELECTALL:   LFILE(i).flags |= FILE_FLAG_MARKED;  break;
            case KEY_FM_DESELECTALL: LFILE(i).flags &= ~FILE_FLAG_MARKED; break;
            case KEY_FM_INVERT_SEL:  LFILE(i).flags ^= FILE_FLAG_MARKED;  break;
            }
        }
    }
    else if (display.view_mode == VIEW_REMOTE)
    {
        for (i=0; i<RNFILES; i++)
        {
            if (!markable (RFILE(i))) continue;
            switch (key)
            {
            case KEY_FM_SELECTALL:   RFILE(i).flags |= FILE_FLAG_MARKED;  break;
            case KEY_FM_DESELECTALL: RFILE(i).flags &= ~FILE_FLAG_MARKED; break;
            case KEY_FM_INVERT_SEL:  RFILE(i).flags ^= FILE_FLAG_MARKED;  break;
            }
        }
    }
}

/* ------------------------------------------------------------- */

//#define XTERM "xterm -ls -fn ssck-14 -bg rgb:a0/a0/a0 -geometry 82x30+14+25"
#define XTERM "xterm -ls -bg rgb:a0/a0/a0 -geometry 82x30+14+25"

void do_enter_dir (int key)
{
    char  *p, buffer[MAX_PATH*3];

    if (display.view_mode == VIEW_LOCAL)
    {
        if (LCURFILE.flags & FILE_FLAG_DIR)
        {
            p = strdup (LCURFILE.name);
            l_chdir (p);
            free (p);
        }
        else if ((LCURFILE.flags & FILE_FLAG_REGULAR) &&
                 (LCURFILE.rights & (S_IXUSR|S_IXGRP|S_IXOTH)))
        {
            buffer[0] = '\0';
            if (fl_opt.has_driveletters)
            {
                buffer[0] = query_drive () + 'a';
                buffer[1] = ':';
                buffer[2] = '\0';
            }
            strcat (buffer, local.dir.name);
            str_cats (buffer, LCURFILE.name);
            strcat (buffer, " ");
            if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;
            fly_launch (buffer, TRUE, TRUE);
            /*
            strcpy (buffer, local.dir.name);
            str_cats (buffer, LCURFILE.name);
            strcat (buffer, " ");
            if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;
            sprintf (buffer2, "%s -e sh -c \"cd %s; %s; echo press enter to continue; read x\"", XTERM, local[pnl].dir.name, buffer);
            fly_launch (buffer2, TRUE, TRUE);
            refresh (pnl);
            */
        }
    }
    else
    {
        if (!(RCURFILE.flags & (FILE_FLAG_DIR|FILE_FLAG_LINK))) return;
        if (site.system_type == SYS_MVS_NATIVE)
        {
            dmsg ("MVS: entering %s from %s\n", RCURFILE.name, site.u.pathname);
            if (strcmp (RCURFILE.name, "..") == 0)
            {
                // check for going to the "top"
                if (strchr (site.u.pathname, '.') == NULL)
                {
                    buffer[0] = '\0';
                    if (entryfield (MSG(M_ENTER_DIRECTORY), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
                        buffer[0] == 0) return;
                    put_message (MSG(M_CHANGING_DIR));
                    r_chdir ("..", FALSE);
                    r_chdir (buffer, FALSE);
                }
                else
                {
                    r_chdir (RCURFILE.name, RCURFILE.flags & FILE_FLAG_MARKED);
                }
            }
            else
            {
                r_chdir (RCURFILE.name, RCURFILE.flags & FILE_FLAG_MARKED);
            }
        }
        else
        {
            r_chdir (RCURFILE.name, RCURFILE.flags & FILE_FLAG_MARKED);
        }
    }
    display.quicksearch[0] = '\0';
}

/* ------------------------------------------------------------- */

void do_transfer (int where)
{
    int   i, j, a, rc, /*oldmode, walked,*/ marked, submarked;
    char  *p;
    int64_t msize;

    if (where == DOWN)
    {
        /* check whether we have marked files */
        marked = 0;
        for (i=0; i<RNFILES; i++)
            if (RFILE(i).flags & FILE_FLAG_MARKED) marked++;

        /* check whether we have marked files in subdirectories */
        submarked = 0;
        msize = 0;
        if (marked == 0)
        {
            p = str_join (RCURDIR.name, "/");
            for (i=0; i<site.ndir; i++)
            {
                if (str_headcmp (site.dir[i].name, p) == 0)
                {
                    for (j=0; j<site.dir[i].nfiles; j++)
                    {
                        if (site.dir[i].files[j].flags & FILE_FLAG_MARKED)
                        {
                            if (site.dir[i].files[j].flags & FILE_FLAG_REGULAR)
                                msize += site.dir[i].files[j].size;
                            submarked++;
                        }
                    }
                }
            }
            free (p);
        }

        /* avoid transferring '..' */
        if (marked == 0 && submarked == 0 &&
            strcmp (RCURFILE.name, "..") == 0) return;

        /* get user confirmation */
        if (marked)
        {
            a = fly_ask (ASK_YN,
                         " Transfer %d selected files/directories \n"
                         " to %s ",
                         NULL, marked, local.dir.name);
        }
        else
        {
            if (submarked)
            {
                a = fly_ask (ASK_YN,
                             " Transfer %d selected files/directories \n"
                             " from subdirectories (total size %s bytes) \n"
                             " to %s ",
                             NULL, submarked, pretty64(msize), local.dir.name);
            }
            else
            {
                a = fly_ask (ASK_YN,
                             " Transfer %s:%u:%s \n"
                             " to %s? ",
                             NULL, site.u.hostname, site.u.port,
                             RCURFILE.name, local.dir.name);
            }
        }
        if (a == 0) return;

        /* when nothing is marked we mark current file/directory */
        if (marked == 0 && submarked == 0)
        {
            if (!markable (RCURFILE)) return;
            RCURFILE.flags |= FILE_FLAG_MARKED;
        }

        /* now we must make sure that subdirectories of marked directories are
         fully buffered */
        for (i=0; i<RNFILES; i++)
        {
            if (RFILE(i).flags & FILE_FLAG_MARKED &&
                RFILE(i).flags & FILE_FLAG_DIR)
            {
                //walked = is_walked (site.cdir, i);
                //dmsg ("walked = %d for %s / %s\n", walked, RCURDIR.name, RFILE(i).name);
                //if (!walked)
                //{
                    //oldmode = site.batch_mode;
                    //site.batch_mode = TRUE;
                    //rc = traverse_directory (RFILE(i).name, TRUE);
                    rc = cache_subtree (site.cdir, i);
                    if (rc == 0) set_dir_size (site.cdir, i);
                    //set_marks (nsite, i);
                    //site.batch_mode = oldmode;
                //1}
            }
        }
        if (submarked)
        {
            p = str_join (RCURDIR.name, "/");
            for (i=0; i<site.ndir; i++)
            {
                if (str_headcmp (site.dir[i].name, p) == 0)
                {
                    for (j=0; j<site.dir[i].nfiles; j++)
                    {
                        if (site.dir[i].files[j].flags & FILE_FLAG_MARKED &&
                            site.dir[i].files[j].flags & FILE_FLAG_DIR)
                        {
                            rc = cache_subtree (i, j);
                            if (rc == 0) set_dir_size (i, j);
                        }
                    }
                }
            }
            free (p);
        }

        // do actual transfer
        do_download_marked ();
    }
    else if (where == UP)
    {
        /* check whether we have marked files */
        marked = 0;
        for (i=0; i<LNFILES; i++)
            if (LFILE(i).flags & FILE_FLAG_MARKED) marked++;

        /* avoid transferring '..' */
        if (marked == 0)
        {
            if (!markable (LCURFILE)) return;
        }

        /* get user confirmation */
        if (marked)
        {
            a = fly_ask (ASK_YN,
                         " Transfer %d selected files/directories \n"
                         " to %s:%d%s ? ",
                         NULL, marked, site.u.hostname, site.u.port,
                         RCURDIR.name);
        }
        else
        {
            a = fly_ask (ASK_YN,
                         " Transfer '%s' \n"
                         " to %s:%d%s ? ",
                         NULL, LCURFILE.name, site.u.hostname, site.u.port,
                         RCURDIR.name);
        }
        if (a == 0) return;

        /* when nothing is marked we mark current file/directory */
        if (marked == 0) LCURFILE.flags |= FILE_FLAG_MARKED;

        do_upload_marked ();
    }
}

/* ------------------------------------------------------------- */

void do_download_marked (void)
{
    int         i, j, l, nt, nta, keep_tree=TRUE;
    trans_item  *T;
    char        *subdir;

    history_add (&site.u);

    // compose transfer list
    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    for (i=0; i<site.ndir; i++)
    {
        dmsg ("checking directory %s\n", site.dir[i].name);
        // skip files outside current tree
        l = strlen (RCURDIR.name);
        if (keep_tree && strncmp (site.dir[i].name, RCURDIR.name, l) != 0) continue;
        if (RCURDIR.name[l-1] == '/') l--;
        subdir = str_sjoin (local.dir.name, site.dir[i].name+l);
        if (strcmp (subdir, "/") != 0) str_strip (subdir, "/");
        for (j=0; j<site.dir[i].nfiles; j++)
        {
            //dmsg ("FILE: %s (%x)\n", site.dir[i].files[j].name, site.dir[i].files[j].flags);
            if (site.dir[i].files[j].flags & FILE_FLAG_MARKED)
            {
                if (nt == nta)
                {
                    nta *= 2;
                    T = xrealloc (T, nta * sizeof (trans_item));
                }

                if (site.dir[i].files[j].flags & FILE_FLAG_REGULAR)
                {
                    T[nt].r_dir  = strdup (site.dir[i].name);
                    T[nt].r_name = strdup (site.dir[i].files[j].name);
                    T[nt].l_dir  = strdup (subdir);
                    T[nt].l_name = strdup (site.dir[i].files[j].name);
                    T[nt].mtime  = site.dir[i].files[j].mtime;
                    T[nt].size   = site.dir[i].files[j].size;
                    T[nt].perm   = site.dir[i].files[j].rights;
                    T[nt].description = site.dir[i].files[j].desc;
                    T[nt].f = &(site.dir[i].files[j]);
                    T[nt].flags = 0;
                    nt++;
                }
                else if (site.dir[i].files[j].flags & FILE_FLAG_DIR)
                {
                    T[nt].r_dir  = strdup (site.dir[i].name);
                    T[nt].r_name = strdup (site.dir[i].files[j].name);
                    T[nt].l_dir  = strdup (subdir);
                    T[nt].l_name = strdup (site.dir[i].files[j].name);
                    T[nt].mtime  = site.dir[i].files[j].mtime;
                    T[nt].size   = 0;
                    T[nt].perm   = site.dir[i].files[j].rights;
                    T[nt].description = site.dir[i].files[j].desc;
                    T[nt].f = &(site.dir[i].files[j]);
                    T[nt].flags = TF_DIRECTORY;
                    nt++;
                }
            }
        }
        free (subdir);
    }

    transfer (DOWN, T, nt, TRUE);

    for (i=0; i<nt; i++)
    {
        free (T[i].r_dir); free (T[i].r_name);
        free (T[i].l_dir); free (T[i].l_name);
    }
    free (T);

    // remove marks
    //unset_marks (RCURDIR.name);
}

/* put files from 'locpath' into queue 'T' to be uploaded to 'rempath' */
static void queue_subdir (trans_item **T, int *nt, int *nta,
                          char *locpath, char *rempath)
{
    directory D;
    int i;
    char  *p1, *p2;

    /* load list of files */
    if (localdir (&D, locpath) < 0) return;

    /* put all directories into queue */
    for (i=0; i<D.nfiles; i++)
    {
        /* skip non-directories and .. */
        if (!(D.files[i].flags & FILE_FLAG_DIR)) continue;
        if (strcmp (D.files[i].name, "..") == 0) continue;

        p1 = str_sjoin (locpath, D.files[i].name);
        p2 = str_sjoin (rempath, D.files[i].name);
        queue_subdir (T, nt, nta, p1, p2);
        free (p1);
        free (p2);

        if (*nt == *nta)
        {
            *nta *= 2;
            *T = xrealloc (*T, *nta * sizeof (trans_item));
        }

        (*T)[*nt].r_dir  = strdup (rempath);
        (*T)[*nt].r_name = strdup (D.files[i].name);
        (*T)[*nt].l_dir  = strdup (locpath);
        (*T)[*nt].l_name = strdup (D.files[i].name);
        (*T)[*nt].mtime  = D.files[i].mtime;
        (*T)[*nt].size   = 0;
        (*T)[*nt].perm   = D.files[i].rights;
        (*T)[*nt].description = D.files[i].desc;
        (*T)[*nt].f = NULL;
        (*T)[*nt].flags = TF_DIRECTORY;
        (*nt)++;
    }

    /* put all files into queue */
    for (i=0; i<D.nfiles; i++)
    {
        /* skip non-files */
        if (!(D.files[i].flags & FILE_FLAG_REGULAR)) continue;

        if (*nt == *nta)
        {
            *nta *= 2;
            *T = xrealloc (*T, *nta * sizeof (trans_item));
        }

        (*T)[*nt].r_dir  = strdup (rempath);
        (*T)[*nt].r_name = strdup (D.files[i].name);
        (*T)[*nt].l_dir  = strdup (locpath);
        (*T)[*nt].l_name = strdup (D.files[i].name);
        (*T)[*nt].mtime  = D.files[i].mtime;
        (*T)[*nt].size   = D.files[i].size;
        (*T)[*nt].perm   = D.files[i].rights;
        (*T)[*nt].description = D.files[i].desc;
        (*T)[*nt].f = NULL;
        (*T)[*nt].flags = 0;
        (*nt)++;
    }

    /* free list of files */
    for (i=0; i<D.nfiles; i++) free (D.files[i].name);
    free (D.files);
    free (D.name);
}

/* ------------------------------------------------------------- */
void do_upload_marked (void)
{
    char        *p, *p1;
    int         i, j, nt, nta, to_cdir;
    trans_item  *T;

    to_cdir = site.cdir;

    /* then we can built a list of files which have to be uploaded */
    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    for (i=0; i<LNFILES; i++)
    {
        if (!(LFILE(i).flags & FILE_FLAG_MARKED)) continue;

        if (LFILE(i).flags & FILE_FLAG_DIR)
        {
            /* queue files from this subdirectory */
            p = str_sjoin (local.dir.name, LFILE(i).name);
            p1 = str_sjoin (RCURDIR.name, LFILE(i).name);
            queue_subdir (&T, &nt, &nta, p, p1);
            /* see if we have corresponding remote directory */
            for (j=0; j<RNFILES; j++)
            {
                if (strcmp (LFILE(i).name, RFILE(j).name) == 0 &&
                    (RFILE(j).flags & FILE_FLAG_DIR))
                {
                    cache_subtree (site.cdir, j);
                    break;
                }
            }
            //lcache_scandir (p);
            free (p);
            free (p1);

            if (nt == nta)
            {
                nta *= 2;
                T = realloc (T, nta * sizeof (trans_item));
            }
            T[nt].r_dir  = strdup (RCURDIR.name);
            T[nt].r_name = strdup (LFILE(i).name);
            T[nt].l_dir  = strdup (local.dir.name);
            T[nt].l_name = strdup (LFILE(i).name);
            T[nt].mtime  = LFILE(i).mtime;
            T[nt].size   = 0;
            T[nt].perm   = LFILE(i).rights;
            T[nt].description = LFILE(i).desc;
            T[nt].f = &(LFILE(i));
            T[nt].flags = TF_DIRECTORY;
            nt++;
        }
        else if (LFILE(i).flags & FILE_FLAG_REGULAR)
        {
            if (nt == nta)
            {
                nta *= 2;
                T = realloc (T, nta * sizeof (trans_item));
            }
            T[nt].r_dir  = strdup (RCURDIR.name);
            T[nt].r_name = strdup (LFILE(i).name);
            T[nt].l_dir  = strdup (local.dir.name);
            T[nt].l_name = strdup (LFILE(i).name);
            T[nt].mtime  = LFILE(i).mtime;
            T[nt].size   = LFILE(i).size;
            T[nt].perm   = LFILE(i).rights;
            T[nt].description = LFILE(i).desc;
            T[nt].f = &(LFILE(i));
            T[nt].flags = 0;
            nt++;
        }
    }

    transfer (UP, T, nt, TRUE);

    for (i=0; i<nt; i++)
    {
        free (T[i].r_dir); free (T[i].r_name);
        free (T[i].l_dir); free (T[i].l_name);
    }
    free (T);

    /* invalidate subdirectories on remote 
    for (i=0; i<RNFILES; i++)
    {
        if (RFILE(i).
    invalidate_directory (site.dir[to_cdir].name, TRUE);
    */

    /* remove marks from local directories (marks from files were
     removed by transfer()) */
    //for (i=0; i<LNFILES; i++)
    //    if ((LFILE(i).flags & FILE_FLAG_MARKED) &&
    //        (LFILE(i).flags & FILE_FLAG_DIR)) LFILE(i).flags &= ~FILE_FLAG_MARKED;

    //lcache_free ();
    ask_refresh (NULL);
}

/* ------------------------------------------------------------- */

void do_local_delete (int key)
{
    int   i, mk, answer, do_all_subdirs, noisy;
    char  buffer[1024], buffer2[512];

    //if (!fly_ask (ASK_YN|ASK_WARN, MSG(M_REALLY_DELETE), NULL)) break;
    // check for selected files
    mk = 0;
    for (i=0; i<LNFILES; i++)
        if (LFILE(i).flags & FILE_FLAG_MARKED) mk++;
    // ask whether to delete
    strcpy (buffer2, "");
    if (mk)
    {
        if (LCURFILENO != LNFILES-1)
            strcpy (buffer2, LFILE(LCURFILENO+1).name);
        else if (LCURFILENO != 0)
            strcpy (buffer2, LFILE(LCURFILENO-1).name);
        answer = fly_ask (ASK_YN, MSG(M_DELETE_FILES), NULL, mk);
    }
    else
    {
        if (LCURFILENO != LNFILES-1)
            strcpy (buffer2, LFILE(LCURFILENO+1).name);
        else if (LCURFILENO != 0)
            strcpy (buffer2, LFILE(LCURFILENO-1).name);
        answer = fly_ask (ASK_YN, MSG(M_FTPS_DELETE), NULL, LCURFILE.name);
    }
    if (answer == 0) return;
    // do the delete
    noisy = TRUE;
    if (mk)
    {
        do_all_subdirs = FALSE;
        for (i=0; i<LNFILES; i++)
        {
            if (!(LFILE(i).flags & FILE_FLAG_MARKED)) continue;
            if (LFILE(i).flags & FILE_FLAG_DIR)
            {
                if (rmdir (LFILE(i).name))
                {
                    if (errno == ENOTEMPTY || errno == EEXIST)
                    {
                        // directory is not empty
                        if (!do_all_subdirs)
                        {
                            snprintf1 (buffer, sizeof(buffer), "%s\n%s\n  %s  ", fl_str.yes, fl_str.no, MSG(M_ALL));
                            answer = fly_ask (ASK_WARN, MSG(M_NOT_EMPTY), buffer, LFILE(i).name);
                            if (answer == 0) return;
                            if (answer == 3) do_all_subdirs = TRUE;
                            if (answer == 2) continue;
                        }
                        delete_subdir (LFILE(i).name, &noisy);
                    }
                    else
                    {
                        // some other error besides NOTEMPTY
                        if (noisy)
                            fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), LFILE(i).name);
                    }
                }
            }
            else
            {
                if (unlink (LFILE(i).name))
                {
                    if (noisy)
                        fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), LFILE(i).name);
                }
            }
        }
    }
    else
    {
        if (LCURFILE.flags & FILE_FLAG_DIR)
        {
            if (rmdir (LCURFILE.name))
            {
                if (errno == ENOTEMPTY || errno == EEXIST)
                {
                    if (fly_ask (ASK_YN|ASK_WARN, MSG(M_NOT_EMPTY), NULL, LCURFILE.name))
                        delete_subdir (LCURFILE.name, &noisy);
                    else
                        return;
                }
                else
                {
                    if (noisy)
                        fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), LCURFILE.name);
                }
            }
        }
        else
        {
            if (unlink (LCURFILE.name))
            {
                dmsg ("errno = %d\n", errno);
                if (errno == EPERM || errno == EACCES)
                {
                    answer = fly_ask (ASK_YN|ASK_WARN, "File `%s' is read-only, delete anyway?", NULL, LCURFILE.name);
                    if (answer == 0) return;
                    if (answer == 1)
                    {
                        chmod (LCURFILE.name, 700);
                        unlink (LCURFILE.name);
                    }
                }
                else
                    fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), LCURFILE.name);
            }
        }
    }
    try_set_local_cursor (buffer2);
    build_local_filelist (NULL);
}

/* ------------------------------------------------------------- */

void do_remote_delete (int key)
{
    int   i, rc, mk, answer;
    int   notempty=TRUE, readonly=TRUE;
    char  *p;

    rc = set_wd (RCURDIR.name, TRUE);
    if (rc) return;

    // check whether we have marked files
    mk = 0;
    for (i=0; i<RNFILES; i++)
        if (RFILE(i).flags & FILE_FLAG_MARKED) mk++;

    /* ask whether to delete */
    if (mk)
        answer = fly_ask (ASK_YN, MSG(M_DELETE_FILES), NULL, mk);
    else
        answer = fly_ask (ASK_YN, MSG(M_FTPS_DELETE), NULL, RCURFILE.name);
    if (answer == 0) return;

    // do the delete
    put_message (MSG(M_DELETING), "");
    if (mk)
    {
        rc = 0;
        /* first we must delete files (they are in the current directory) */
        for (i=0; i<RNFILES; i++)
        {
            if ((RFILE(i).flags & FILE_FLAG_MARKED) &&
                (RFILE(i).flags & FILE_FLAG_REGULAR))
            {
                rc = delete_remote_file (RCURDIR.name, RFILE(i).name, &readonly);
                if (rc == ERR_FATAL || rc == ERR_CANCELLED)
                {
                    ask_refresh (NULL);
                    return;
                }
            }
        }

        /* now delete directories */
        for (i=0; i<RNFILES; i++)
        {
            if ((RFILE(i).flags & FILE_FLAG_MARKED) &&
                (RFILE(i).flags & FILE_FLAG_DIR))
            {
                rc = delete_remote_subdir (RCURDIR.name, RFILE(i).name, &notempty, &readonly);
                p = str_sjoin (RCURDIR.name, RFILE(i).name);
                invalidate_directory (p, TRUE);
                free (p);
                if (rc == ERR_FATAL || rc == ERR_CANCELLED)
                {
                    ask_refresh (NULL);
                    return;
                }
            }
        }
    }
    else
    {
        /* delete single entry under cursor */
        if (RCURFILE.flags & FILE_FLAG_DIR)
        {
            /* delete directory under cursor */
            delete_remote_subdir (RCURDIR.name, RCURFILE.name, &notempty, &readonly);
            p = str_sjoin (RCURDIR.name, RCURFILE.name);
            invalidate_directory (p, TRUE);
            free (p);
        }
        else
        {
            /* delete file under cursor */
            delete_remote_file (RCURDIR.name, RCURFILE.name, &readonly);
        }
    }

    ask_refresh (NULL);
}

/* this function deletes one file 'name' from directory 'dir'. return
 codes:
 ERR_OK -- file has been deleted successfully
 ERR_SKIP -- file has been skipped (couldn't delete it)
 ERR_FATAL -- something bad has occured (SendToServer has returned it)
 ERR_CANCELLED -- user has cancelled operation */
int delete_remote_file (char *dirname, char *filename, int *noisy)
{
    int rc, a, rsp;
    char  answers[64], response[8192];

    rc = set_wd (dirname, FALSE);
    if (rc) return rc;

    rc = SendToServer (TRUE, &rsp, response, "DELE %s", filename);
    if (rc) return rc;
    if (rsp != 2)
    {
        if (*noisy)
        {
            snprintf1 (answers, sizeof(answers), "%s\n%s\n  %s  ",
                       MSG(M_OK), "  OK to all  ", MSG(M_CANCEL));
            a = fly_ask (ASK_WARN,
                         " Cannot delete '%s/%s': \n"
                         " %s ",
                         answers, dirname, filename, response);
            if (a == 2) *noisy = FALSE;
            if (a == 0 || a == 3) return ERR_CANCELLED;

        }
        return ERR_SKIP;
    }

    return ERR_OK;
}

/* delete everything on remote site under path dirname/filename.
 (including this path). return codes:
 ERR_OK -- success (directory has been totally removed);
 ERR_SKIP -- when errors prevented this directory from deleting
 (protected files, not enough permissions etc.);
 ERR_CANCELLED -- if user has cancelled deletion;
 ERR_FATAL -- on severe errors.
 when 'noisy' is TRUE we ask user when decision is required,
 when 'noisy' is FALSE we skip confirmations */
int delete_remote_subdir (char *dirname, char *filename,
                          int *noisy_notempty, int *noisy_readonly)
{
    char  answers[1024], *path;
    int   i, a, d, rc, rc1, rc2, rsp;

    path = str_sjoin (dirname, filename);
    dmsg ("going to delete directory %s\n", path);

    if (!options.slowlink)
        put_message (MSG(M_DELETING), path);

    /* first we try to change to directory one level up this one
     and try to delete given directory. this will fail if it is not empty.
     we do that because on DOSish systems you can't delete your current
     directory */
    rc = set_wd (dirname, TRUE);
    if (rc)
    {
        free (path);
        return rc;
    }

    rc = SendToServer (TRUE, &rsp, NULL, "RMD %s", filename);
    if (rc)
    {
        free (path);
        return rc;
    }

    /* if we succeed we only have to remove directory from the cache */
    if (rsp == 2)
    {
        //invalidate_directory (path, FALSE);
        free (path);
        return ERR_OK;
    }

    /* directory cannot be removed (probably it is not empty). ask user
     what to do if needed */
    if (*noisy_notempty)
    {
        snprintf1 (answers, sizeof(answers),
                   "%s\n%s\n  %s  ", fl_str.yes, fl_str.no, MSG(M_ALL));
        a = fly_ask (ASK_WARN, MSG(M_NOT_EMPTY), answers, path);
        if (a == 0 || a == 2)
        {
            free (path);
            return ERR_CANCELLED; /* esc, no */
        }
        if (a == 3) *noisy_notempty = FALSE; /* all */
    }

    /* make sure remote directory is cached */
    if (cache_dir (path) != 0)
    {
        if (*noisy_readonly)
        {
            a = fly_ask_ok_cancel (ASK_WARN, " Cannot enter directory %s ",
                                   path);
            if (a == 0 || a == 2)
            {
                free (path);
                return ERR_CANCELLED;
            }
        }
        free (path);
        return ERR_SKIP;
    }

    /* find out the remote directory those contents we are going to delete */
    d = find_directory (path);
    if (d < 0) return ERR_FATAL;

    /* change to this directory */
    rc = set_wd (path, FALSE);
    if (rc)
    {
        free (path);
        return rc; /* shouldn't happen; just sanity check */
    }

    /* first we delete all regular files in this directory.
     we're in this directory */
    dmsg ("deleting files\n");
    rc1 = 0;
    for (i=0; i<site.dir[d].nfiles; i++)
    {
        if (site.dir[d].files[i].flags & FILE_FLAG_REGULAR)
        {
            rc = delete_remote_file (site.dir[d].name,
                                     site.dir[d].files[i].name, noisy_readonly);
            if (rc == ERR_FATAL || rc == ERR_CANCELLED)
            {
                free (path);
                //invalidate_directory (path, FALSE);
                return rc;
            }
            if (rc) rc1 = rc; /* if some files were skipped remember that */
        }
    }

    /* ... and then we delete all subdirectories */
    dmsg ("deleting directories\n");
    rc2 = 0;
    for (i=0; i<site.dir[d].nfiles; i++)
    {
        if (strcmp (site.dir[d].files[i].name, ".") == 0 ||
            strcmp (site.dir[d].files[i].name, "..") == 0) continue;
        if (site.dir[d].files[i].flags & FILE_FLAG_DIR)
        {
            rc = delete_remote_subdir (site.dir[d].name, site.dir[d].files[i].name,
                                       noisy_notempty, noisy_readonly);
            dmsg ("delete_remote_subdir(%s/%s) has returned %d\n",
                  site.dir[d].name, site.dir[d].files[i].name, rc);
            if (rc == ERR_FATAL || rc == ERR_CANCELLED)
            {
                //invalidate_directory (path, FALSE);
                free (path);
                return rc;
            }
            if (rc) rc2 = rc; /* if some directories were skipped remember that */
        }
    }

    /* again we try to change to directory one level up this one
     and try to delete given directory. it this fails we don't do anything,
     just return an error */
    rc = set_wd (dirname, TRUE);
    if (rc)
    {
        //invalidate_directory (path, FALSE);
        free (path);
        return rc;
    }

    rc = SendToServer (TRUE, &rsp, NULL, "RMD %s", filename);
    if (rc)
    {
        //invalidate_directory (path, FALSE);
        free (path);
        return rc;
    }

    /* invalidate cached directory */
    dmsg ("invalidating cached directory %s\n", dirname);
    //invalidate_directory (dirname, FALSE);
    free (path);

    /* success? */
    if (rsp != 2) return ERR_SKIP;

    dmsg ("successfully deleted %s/%s\n", dirname, filename);
    //return change_dir (dirname);
    return ERR_OK;
}

