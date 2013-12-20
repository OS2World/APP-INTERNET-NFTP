#include <fly/fly.h>
#include <manycode.h>

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
#include "auxprogs.h"
#include "network.h"
#include "passwords.h"
#include "mirror.h"
#include "do_key.h"

#ifndef SIGTSTP
#define SIGTSTP 20
#endif

static flyfont *F = NULL;
static int nf = 0;

/* ------------------------------------------------------------- */

struct
{
    int  key;
    void (*function)(int key);
}
actions[] =
{
    {KEY_EDIT_PASSWORDS,      psw_edit},
    {KEY_CHANGE_KEYPHRASE,    psw_chkey},
    {KEY_CRASH,               NULL},
    {KEY_GEN_GOTO,            do_select_panel_contents},

    {KEY_FM_ENTER_DIR,        do_enter_dir},
    {KEY_ENTER,               do_enter_dir},
    {KEY_FM_VIEW_INT,         do_view},
    {KEY_FM_VIEW_EXT,         do_view},
    {KEY_FM_MKDIR,            do_mkdir},
    {KEY_FM_DELETE,           do_delete},
    {KEY_FM_CHDIR,            do_chdir},
    {KEY_COPY,                do_copy},
    {KEY_MOVE,                do_move},
    {KEY_FM_RENAME,           do_rename},
    {KEY_CHANGE_PERMISSIONS,  do_chmod},
    {KEY_FM_REREAD,           do_reread},
    {KEY_FM_GO_ROOT,          do_go_root},
    {KEY_FM_LOAD_DESCRIPTIONS, do_load_descriptions},
    {KEY_FM_COMPUTE_DIRSIZE,  do_walk},
    {KEY_FM_DOWNLOAD_WGET,    do_launch_wget},

    {KEY_FM_SORT_NAME,        do_sort},
    {KEY_FM_SORT_EXT,         do_sort},
    {KEY_FM_SORT_TIME,        do_sort},
    {KEY_FM_SORT_SIZE,        do_sort},
    {KEY_FM_SORT_UNSORT,      do_sort},
    {KEY_FM_SORT_REVERSE,     do_sort},

    {KEY_FM_SELECT,           do_mark},
    {KEY_FM_MASK_SELECT,      do_mark_by_mask},
    {KEY_FM_MASK_DESELECT,    do_mark_by_mask},
    {KEY_FM_SELECTALL,        do_mark_multiple},
    {KEY_FM_DESELECTALL,      do_mark_multiple},
    {KEY_FM_INVERT_SEL,       do_mark_multiple},

    {KEY_FTPS_DEFINE_SERVERS, (void *)ftpsearch_servers},
    {KEY_GEN_SWITCH_TO_CC,    (void *)do_show_cc},
    {KEY_SELECT_LANGUAGE,     do_select_language},
    {KEY_EXPORT_MARKED,       do_export_marked},
    {KEY_FM_SAVE_LISTING,     do_save_listing},
    {KEY_SELECT_FONT,         do_select_font},
    {KEY_GEN_EXIT,            do_exit},
    {KEY_GEN_EDIT_INI,        do_edit_ini},
    {KEY_GEN_LOGOFF,          do_logoff},
    {KEY_GEN_QUOTE,           do_quote},
    {KEY_BINARY_ASCII,        do_binascii},

    {KEY_FTPS_FIND,           do_ftps_find},
    {KEY_FTPS_RECALL,         do_ftps_recall},
};

struct
{
    int key;
    int *sw;
}
switches[] =
{
    {KEY_SYNC_DELETE,        &status.sync_delete},
    {KEY_PAUSE_TRANSFER,     &status.transfer_paused},
    {KEY_GEN_SPLITSCREEN,    &status.split_view},
    {KEY_GEN_SHOWTIMESTAMPS, &status.show_times},
    {KEY_GEN_SHOWMINISTATUS, &status.show_ministatus},
    {KEY_SHOW_OWNER,         &status.show_owner},
    {KEY_SYNC_SUBDIRS,       &status.sync_subdirs},
    //{KEY_TRAFFIC_LIMIT,      &status.traffic_shaping},
};

/* ------------------------------------------------------------- */

void drawline_select_font (int n, int len, int shift, int pointer, int row, int col)
{
    int a = pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back;
    char buffer[128];

    video_put_n_cell (' ', a, len, row, col);
    if (fl_opt.platform == PLATFORM_OS2_PM)
        sprintf (buffer, MSG(M_FONT_DESC), F[n].cx, F[n].cy, F[n].name);
    else
        sprintf (buffer, "%2d x %2d %s", F[n].cx, F[n].cy, F[n].name);
    video_put_str (buffer, min1 (strlen (buffer), len-1), row, col+1);
}

/* ------------------------------------------------------------- */
#include "languages.h"

void drawline_select_language (int n, int len, int shift, int pointer, int row, int col)
{
    int a = pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back;

    video_put_n_cell (' ', a, len, row, col);
    video_put_str_attr (lang_names[n], min1 (strlen (lang_names[n]), len-1), row, col+1, a);
}

/* ------------------------------------------------------------- */

void drawline_conn (int n, int len, int shift, int pointer, int row, int col)
{
    int attr;
    char buf[8192];

    if (site[n].set_up)
    {
        sprintf (buf, " FTP %s%s%s:%s",
                 is_anonymous(n) ? "" : site[n].u.userid,
                 is_anonymous(n) ? "" : "@",
                 site[n].u.hostname, site[n].dir[site[n].cdir].name);
        attr = pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back;
        video_put_n_cell (' ', attr, len, row, col);
        len = min1 (len, strlen(buf));
        video_put_str (buf, len, row, col);
    }
    else
    {
        attr = pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back;
        video_put_n_cell (' ', attr, len, row, col);
        video_put (" <EMPTY>", row, col);
    }
}

/* ------------------------------------------------------------- */
enum {WT_LOCALPATH, WT_DRIVE, WT_FTP, WT_NEWFTP, WT_NEWPATH};
typedef struct
{
    int   type;
    int   nsite; /* number of site (WT_FTP) */
    char  *path; /* local path (WT_LOCALPATH) */
    int   drive; /* local drive (WT_DRIVE) */
}
way;

way *ways;

void drawline_goto (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[1024], attr;

    attr = pointer ? options.attr_bmrk_pointer : options.attr_bmrk_back;
    switch (ways[n].type)
    {
    case WT_LOCALPATH:
        sprintf (buf, " %s", ways[n].path);
        break;
    case WT_DRIVE:
        sprintf (buf, " %c:", 'A'+ways[n].drive);
        break;
    case WT_FTP:
        n = ways[n].nsite;
        if (is_anonymous(n))
            sprintf (buf, " FTP %s:%s", site[n].u.hostname, site[n].u.pathname);
        else
            sprintf (buf, " FTP %s@%s:%s", site[n].u.userid, site[n].u.hostname, site[n].u.pathname);
        break;
    case WT_NEWFTP:
        //attr |= _High;
        strcpy (buf, " New FTP connection");
        break;
    case WT_NEWPATH:
        //attr |= _High;
        strcpy (buf, " Change directory to...");
        break;
    }

    video_put_n_cell (' ', attr, len, row, col);
    len = min1 (len, strlen(buf));
    video_put_str (buf, len, row, col);
}

/* ------------------------------------------------------------- */

void do_key (int key)
{
    int    rc = 0, action = KEY_NOTHING;
    char   buffer[MAX_PATH], *p;
    int    a, i, rsp, ns, nsite;
    url_t    u;
    time_t now;

    if (key == 0) goto done;

    if (IS_KEY (key))
    {
        action = options.keytable[key];
        if (action == KEY_NOTHING)
            action = options.keytable2[key];
    }

    if (IS_SYSTEM (key))
    {
        switch (SYS_TYPE(key))
        {
        case SYSTEM_RESIZE:
            update (1);
            action = KEY_NOTHING;          break;
        case SYSTEM_QUIT:
            action = KEY_GEN_EXIT;         break;
        default:
            action = KEY_NOTHING;
        }
    }

    if (IS_MENU (key)) action = key - FMSG_BASE_MENU;

    for (i=0; i<sizeof(actions)/sizeof(actions[0]); i++)
    {
        if (actions[i].key == action)
        {
            actions[i].function (action);
            goto done;
        }
    }

    for (i=0; i<sizeof(switches)/sizeof(switches[0]); i++)
    {
        if (switches[i].key == action)
        {
            *(switches[i].sw) = !*(switches[i].sw);
            goto done;
        }
    }

    switch (action)
    {
    case KEY_MAINHELP:
        help (M_HLP_MAIN);
        break;

    case KEY_SHORTHELP:
        help (M_HLP_SHORT);
        break;

    case KEY_USING_MENUS:
        help (M_HLP_USING_MENU);
        break;

    case KEY_MENU:
        if (!fl_opt.has_osmenu)
        {
            a = menu_process_line (main_menu, options.menu_open_sub,
                                   options.menu_open_sub != -1);
            if (a > 0) do_key (FMSG_BASE_MENU + a);
        }
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
        if (Login (-1, &u, display.cursor)) break;
        if (options.login_bell) Bell (3);
        break;

    case KEY_GEN_BOOKMARKS:
        init_url (&u);
        //if (site[current_site].set_up)
        //    strcpy (u.hostname, site[current_site].u.hostname);
        if (!bookmark_select (&u)) break;
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        //if (entryfield (MSG(M_SELECT_DIRECTORY), u.pathname, sizeof(u.pathname), 0) == PRESSED_ESC) break;
        if (Login (-1, &u, display.cursor)) break;
        if (options.login_bell) Bell (3);
        break;

    case KEY_GEN_HISTORY:
        init_url (&u);
        //strcpy (u.hostname, site[current_site].u.hostname);
        if (history_select (&u) < 0) break;
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        if (Login (-1, &u, display.cursor)) break;
        if (options.login_bell) Bell (3);
        break;

    case KEY_GEN_LOGIN:
        init_url (&u);
        //if (site[current_site].set_up) u = site[current_site].u;
        u.userid[0] = '\0';
        u.password[0] = '\0';
        u.pathname[0] = '\0';
        if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
        if (Login (-1, &u, display.cursor)) break;
        if (options.login_bell) Bell (3);
        break;

    case KEY_UPDATE_NFTP:
        init_url (&u);
        strcpy (u.hostname, "ftp.ayukov.com");
        strcpy (u.pathname, "/pub/nftp");
        if (Login (-1, &u, display.cursor)) break;
        if (options.login_bell) Bell (3);
        break;

    case KEY_GEN_SCREENREDRAW:
        video_update (1);
        break;

    case KEY_INTERNAL_VERSION:
            fly_ask_ok (0,
                        "  NFTP: New File Transfer Protocol Client  \n"
                        "  Version%scompiled %s, %s \n"
                        "  Platform: %s\n"
                        "  Copyright (C) Sergey Ayukov 1994--2000 \n"
                        NFTP_VERSION, __DATE__, __TIME__, fl_opt.platform_name);
        break;

    case KEY_PASSIVE_MODE:
        if (display.view[display.cursor] < 0) break;
        ns = display.view[display.cursor];
        status.passive_mode[ns] = !status.passive_mode[ns];
        break;

    case KEY_GEN_NOOP:
        if (display.view[display.cursor] < 0) break;
        ns = display.view[display.cursor];
        if (is_http_proxy(ns))
        {
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
            break;
        }
        put_message (MSG(M_SENDING_NOOP));
        rc = SendToServer (ns, TRUE, &rsp, NULL, "NOOP");
        update (1);
        if (rc == 0 && rsp != 2) SendToServer (ns, TRUE, &rsp, NULL, "NOOP");
        if (site[ns].connected)
            fly_ask_ok (0, MSG(M_CONNECTION_OK));
        break;

    case KEY_GEN_USEFLAGS:
        if (display.view[display.cursor] < 0) break;
        ns = display.view[display.cursor];
        status.use_flags[ns] = !status.use_flags[ns];
        break;

    case KEY_RESOLVE_SYMLINKS:
        if (display.view[display.cursor] < 0) break;
        ns = display.view[display.cursor];
        status.resolve_symlinks[ns] = !status.resolve_symlinks[ns];
        if (status.resolve_symlinks[ns] && !status.use_flags[ns]) status.use_flags[ns] = TRUE;
        break;

    case KEY_GEN_MODE_SWITCH:
        status.split_view = FALSE;
        display.parsed = !display.parsed;
        break;

    case KEY_BACKSPACE:
        if (display.quicksearch[0] != '\0')
            display.quicksearch[strlen(display.quicksearch)-1] = '\0';
        break;

    case KEY_ESC:
        display.quicksearch[0] = '\0';
        break;

    case KEY_GEN_SWITCH_LOCREM:
        display.cursor = display.cursor == V_LEFT ? V_RIGHT : V_LEFT;
        break;

    case KEY_SUSPEND:
        if (!fl_opt.is_unix || fl_opt.is_x11)
        {
            fly_ask_ok (0, MSG(M_UNSUPPORTED), fl_opt.platform_name);
            break;
        }
        raise (SIGTSTP);
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

    case KEY_UP:
    case KEY_DOWN:
    case KEY_HOME:
    case KEY_END:
    case KEY_PGUP:
    case KEY_PGDN:
        do_scroll (key);
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

    case KEY_FM_GO_UP:
        if (display.view[display.cursor] < 0)
        {
            l_chdir (display.cursor, "..");
        }
        else
        {
            put_message (MSG(M_GOING_UP));
            r_chdir (display.view[display.cursor], "..", FALSE);
        }
        display.quicksearch[0] = '\0';
        break;

    default:
        a = menu_check_key (main_menu, key);
        if (a > 0)
        {
            do_key (FMSG_BASE_MENU + a);
            break;
        }
        // handle quicksearch
        if (!IS_KEY (key)) break;
        if (key > 255) break;
        if (strchr ("0123456789abcdefghijklmnopqrstuvwxyz.-_", key) != NULL)
        {
            if (strlen(display.quicksearch) == sizeof(display.quicksearch)-1)
                break;
            buffer[0] = key; buffer[1] = '\0';
            strcat (display.quicksearch, buffer);
            if (display.view[display.cursor] < 0)
            {
                quicksearch (&local[display.cursor].dir);
                local[display.cursor].dir.first =
                    max1 (local[display.cursor].dir.current-p_nlf()/2, 0);
            }
            else
            {
                nsite = display.view[display.cursor];
                quicksearch (&RN_CURDIR);
                RN_CURDIR.first = max1 (RN_CURDIR.current-p_nlf()/2, 0);
            }
        }
    }

done:
    update_switches ();
    update (1);

    if (options.auto_refresh)
    {
        now = time (NULL);
        for (nsite=0; nsite<MAX_SITE; nsite++)
        {
            if (site[nsite].set_up && site[nsite].updated != -1 &&
                now-site[nsite].updated > options.auto_refresh)
            {
                p = strdup (RN_CURFILE.name);
                rc = set_wd (nsite, RN_CURDIR.name, TRUE);
                if (rc) continue;
                rc = retrieve_dir (nsite, RN_CURDIR.name, site[nsite].cdir);
                if (rc) continue;
                adjust_cursor (&RN_CURDIR, p);
                free (p);
            }
        }
    }
}

/* ------------------------------------------------------------- */

void do_select_language (int key)
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
        video_update (1);
    }
}

/* ------------------------------------------------------------- */

void do_view (int key)
{
    char           tmpfile[MAX_PATH];
    char           buf[MAX_PATH*2+10], *p, *p1;
    int            fh, rc1, rc2, rc, ln, nsite, browser;
    struct stat    before, after;
    trans_item     T;

    if (key == KEY_FM_VIEW_INT) browser = BROWSER_INTERNAL;
    else                        browser = BROWSER_EXTERNAL;

    if (display.view[display.cursor] < 0)
    {
        if (!(LCURFILE(display.cursor).flags & FILE_FLAG_REGULAR)) return;
        p1 = str_sjoin (local[display.cursor].dir.name, LCURFILE(display.cursor).name);
        if (browser == BROWSER_INTERNAL)
        {
            ln = load_file (p1, &p);
            if (ln > 0)
            {
                /* try to convert buffer into koi8 if needed.
                 avoid rcd_guess on huge buffers (awful performance!) */
                //if (ln > 512) {c = p[511]; p[511] = '\0'; }
                //enc = rcd_guess (p);
                //if (ln > 512) p[511] = c;
                //if (enc != EN_UNKNOWN && enc != EN_KOI8R)
                //    ru_convert (p, enc);

                browse_buffer (p, ln, LCURFILE(display.cursor).name);
                free (p);
            }
        }
        else
        {
            sprintf (buf, "%s %s", options.texteditor, p1);
            fly_launch (buf, TRUE, FALSE);
        }
        free (p1);
        return;
    }

    nsite = display.view[display.cursor];
    if (!(RN_CURFILE.flags & FILE_FLAG_REGULAR)) return;

    if (RN_CURFILE.cached == NULL)
    {
        if (RN_CURFILE.size > 512*1024 &&
            !fly_ask (ASK_YN|ASK_WARN, MSG(M_TOO_BIG_FOR_INTVIEWER), NULL,
                      RN_CURFILE.name, RN_CURFILE.size)) return;
        rc = cache_file (nsite, site[nsite].cdir, RN_CURRENT);
        if (rc)
        {
            fly_ask_ok (ASK_WARN, MSG(M_TRANSFER_FAILED), RN_CURFILE.name);
            return;
        }
    }

    if (browser == BROWSER_INTERNAL)
    {
        browse_buffer (RN_CURFILE.cached, RN_CURFILE.size, RN_CURFILE.name);
    }
    else
    {
        tmpnam1 (tmpfile);
        fh = open (tmpfile, O_RDWR|O_CREAT|O_TRUNC|BINMODE, 0600);
        if (fh < 0) return;
        write (fh, RN_CURFILE.cached, RN_CURFILE.size);
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

            T.r_dir  = strdup (RN_CURDIR.name);
            T.l_dir  = strdup (tmpfile);
            T.r_name = strdup (RN_CURFILE.name);
            T.l_name = strdup (p+1);
            T.mtime  = gm2local (after.st_mtime);
            T.size   = after.st_size;
            T.perm   = after.st_mode;
            T.description = NULL;
            T.f = NULL;
            T.flags = TF_OVERWRITE;

            transfer (UP, nsite, -1, &T, 1, TRUE);

            ask_refresh (nsite, NULL);
            if (RN_CURFILE.cached != NULL)
            {
                free (RN_CURFILE.cached);
                RN_CURFILE.cached = NULL;
            }
        }
        remove (tmpfile);
    }
}

/* ------------------------------------------------------------- */

void do_mkdir (int key)
{
    int  rc;
    char buffer[2048];

    if (display.view[display.cursor] >=0 && is_http_proxy(display.view[display.cursor]))
    {
        fly_ask_ok (0, MSG(M_HTTP_PROXY));
        return;
    }

    buffer[0] = '\0';
    if (entryfield (MSG(M_MKDIR_ENTER_NAME), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
        buffer[0] == '\0') return;

    rc = create_directory (buffer);
    /*
    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        if (is_absolute (buffer))
        {
            p = strdup (buffer);
        }
        else
        {
            p = str_sjoin (local[pnl].dir.name, buffer);
        }
        rc = mkdir (p, 0777);
        if (rc)
            fly_ask_ok (ASK_WARN, MSG(M_CANT_CREATE_DIRECTORY), p);
        else
            rc = l_chdir (pnl, NULL);
        adjust_cursor (&local[pnl].dir, buffer);
        free (p);
    }
    else
    {
        put_message (MSG(M_MKDIR_CREATING));
        nsite = display.view[display.cursor];
        rc = set_wd (nsite, RN_CURDIR.name, FALSE);
        if (rc) return;
        rc = SendToServer (nsite, TRUE, &rsp, NULL, "MKD %s", buffer);

        if (rc != 0 || rsp != 2)
            fly_ask_ok (ASK_WARN, MSG(M_CANT_CREATE_DIRECTORY), buffer);
        else
            ask_refresh (nsite, buffer);
    }
    display.quicksearch[0] = '\0';
    */

    if (rc == 0 && options.create_enters)
    {
        do_enter_dir (KEY_FM_ENTER_DIR);
    }
}

/* ------------------------------------------------------------- */
void do_show_cc (int key)
{
   char       buf[1024], *p;
   int        n0, i, r1, r2, c1, c2, nl, nc, fline, shift;
   int        k, action, done, once;
   int        ns, cs, cs1;
   char       attr;
   struct tm  *t1;

   once = (key == 0) ? TRUE : FALSE;
   if (!once)
       menu_disable ();
   if (once) done = TRUE;
   else      done = FALSE;
   fline = -1;
   shift = 0;
   cs = 0; //!!!

   do
   {
       update (0);
       r1 = 3;
       r2 = video_vsize() - 4;
       c1 = 3;
       c2 = video_hsize() - 4;
       nl = r2-r1+1-2; /* number of lines used to display CC lines */
       nc = c2-c1+1-2; /* number of columns used to display CC lines */
       //video_clear (r1, c1, r2, c2, options.attr_cntr_header);
       video_clear (r1, c1, r2, c2, options.attr_cntr_resp);
       //fly_drawframe (r1, c1, r2, c2);
       //fly_shadow (r1, c1, r2, c2);

       /* n0 is an index of the first displayed line in CC[ptr] */
       if (fline == -1)
           n0 = max1 (site[cs].CC.n - nl, 0);
       else
           n0 = fline;

       for (i=0; i<nl; i++)
       {
           if (n0+i < site[cs].CC.n)
           {
               p = site[cs].CC.ptr[n0+i];
               switch (*p)
               {
               case '1': attr = options.attr_cntr_resp; break;
               case '2': attr = options.attr_cntr_cmd; break;
               case '3':
               default:  attr = options.attr_cntr_comment; break;
               }

               memset (buf, ' ', sizeof (buf));
               if (strlen (p+1) > shift)
                   strncpy (buf, p+1+shift, nc);
               if (options.show_cc_time)
               {
                   video_put_str_attr (buf, nc-9, r1+1+i, c1+1+9, attr);
                   t1 = localtime (&(site[cs].CC.tms[n0+i]));
                   sprintf (buf, "%02d:%02d:%02d ", t1->tm_hour, t1->tm_min, t1->tm_sec);
                   video_put_str_attr (buf, 9, r1+1+i, c1+1, options.attr_cntr_resp);
               }
               else
               {
                   video_put_str_attr (buf, nc, r1+1+i, c1+1, attr);
               }
           }
           else
               video_put_n_cell (' ', options.attr_cntr_resp, nc, r1+1+i, c1+1);
       }
       /* header */
       video_put_n_cell (' ', options.attr_cntr_header, c2-c1+1, r1, c1);
       sprintf (buf, "Control Connection (%d: %s)", cs, site[cs].set_up ? site[cs].u.hostname : "");
       video_put (buf, r1, c1+1);

       if (once)
       {
           video_update (FALSE);
           break;
       }

       /* footer */
       video_put_n_cell (' ', options.attr_cntr_header, c2-c1+1, r2, c1);
       video_put ("Esc=Close Ctrl-S=Select site", r2, c1+1);

       video_update (FALSE);
       k = getmessage (-1);

       action = KEY_NOTHING;
       if (IS_KEY (k))
       {
           action = options.keytable[k];
           if (action == KEY_NOTHING)
               action = options.keytable2[k];
       }

       switch (action)
       {
       case KEY_HELP:
           help (M_HLP_CC);
           break;

       case KEY_SELECT_CC:
           ns = 0;
           while (site[ns].set_up) ns++;
           if (ns == 0) break;
           cs1 = fly_choose ("Choose connection",
                             0, &ns, cs, drawline_conn, NULL);
           if (cs1 >= 0)
           {
               fline = -1;
               shift = 0;
               cs = cs1;
           }
           break;

       case KEY_UP:
           if (fline == -1)
               fline = max1 (site[cs].CC.n - nl, 0);
           fline = max1 (0, fline-1);
           break;

       case KEY_DOWN:
           if (fline == -1)
               fline = max1 (site[cs].CC.n - nl, 0);
           fline = min1 (site[cs].CC.n-1, fline+1);
           break;

       case KEY_PGUP:
           if (fline == -1)
               fline = max1 (site[cs].CC.n - nl, 0);
           fline = max1 (fline-nl, 0);
           break;

       case KEY_PGDN:
           if (fline == -1)
               fline = max1 (site[cs].CC.n - nl, 0);
           fline = min1 (fline+nl, max1 (site[cs].CC.n - nl, 0));
           break;

       case KEY_LEFT:
           if (shift > 0) shift--;
           break;

       case KEY_PG_LEFT:
           shift = max1 (0, shift-10);
           break;

       case KEY_RIGHT:
           shift++;
           break;

       case KEY_PG_RIGHT:
           shift += 10;
           break;

       case KEY_HOME:
           fline = 0;
           shift = 0;
           break;

       case KEY_END:
           fline = -1;
           shift = 0;
           break;

       case KEY_ESC:
           done = TRUE;
       }
   }
   while (!done);
   if (!once)
       menu_enable ();
}

/* ------------------------------------------------------------- */

void do_delete (int key)
{
    int            i, mk, rc, pnl, nsite, answer, noisy, do_all_subdirs;
    char           buffer[2048], *pdel, *prevfile;

    if (display.view[display.cursor] >= 0 && is_http_proxy(display.view[display.cursor]))
    {
        fly_ask_ok (0, MSG(M_HTTP_PROXY));
        return;
    }

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        // check for selected files
        mk = 0;
        for (i=0; i<LNFILES(pnl); i++)
            if (LFILE(pnl, i).flags & FILE_FLAG_MARKED) mk++;
        // ask whether to delete
        prevfile = NULL;
        if (mk)
        {
            if (LCURFILENO(pnl) != LNFILES(pnl)-1)
                prevfile = LFILE(pnl, LCURFILENO(pnl)+1).name;
            else if (LCURFILENO(pnl) != 0)
                prevfile = LFILE(pnl, LCURFILENO(pnl)-1).name;
            answer = fly_ask (ASK_YN, MSG(M_DELETE_FILES), NULL, mk);
        }
        else
        {
            if (strcmp (LCURFILE(pnl).name, "..") == 0) return;
            answer = fly_ask (ASK_YN, MSG(M_FTPS_DELETE), NULL, LCURFILE(pnl).name);
        }
        if (answer == 0) return;
        // do the delete
        noisy = TRUE;
        if (mk)
        {
            do_all_subdirs = FALSE;
            for (i=0; i<LNFILES(pnl); i++)
            {
                if (!(LFILE(pnl,i).flags & FILE_FLAG_MARKED)) continue;
                pdel = str_sjoin (local[pnl].dir.name, LFILE(pnl,i).name);

                if (LFILE(pnl,i).flags & FILE_FLAG_DIR)
                {
                    if (rmdir (pdel))
                    {
                        if (errno == ENOTEMPTY || errno == EEXIST)
                        {
                            // directory is not empty
                            if (!do_all_subdirs)
                            {
                                sprintf (buffer, "%s\n%s\n  %s  ", fl_str.yes, fl_str.no, MSG(M_ALL));
                                answer = fly_ask (ASK_WARN, MSG(M_NOT_EMPTY), buffer, LFILE(pnl,i).name);
                                if (answer == 0) {free (pdel); return;}
                                if (answer == 3) do_all_subdirs = TRUE;
                                if (answer == 2) {free (pdel); continue;}
                            }
                            delete_subdir (pdel, &noisy);
                        }
                        else
                        {
                            // some other error besides NOTEMPTY
                            if (noisy)
                                fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                        }
                    }
                }
                else
                {
                    if (unlink (pdel))
                    {
                        if (noisy)
                            fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                    }
                }
                free (pdel);
            }
        }
        else
        {
            pdel = str_sjoin (local[pnl].dir.name, LCURFILE(pnl).name);
            if (LCURFILE(pnl).flags & FILE_FLAG_DIR)
            {
                if (rmdir (pdel))
                {
                    if (errno == ENOTEMPTY || errno == EEXIST)
                    {
                        if (fly_ask (ASK_YN|ASK_WARN, MSG(M_NOT_EMPTY), NULL, pdel))
                            delete_subdir (pdel, &noisy);
                        else
                        {
                            free (pdel);
                            return;
                        }
                    }
                    else
                    {
                        if (noisy)
                            fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                    }
                }
            }
            else
            {
                if (unlink (pdel))
                {
                    dmsg ("errno = %d\n", errno);
                    if (errno == EPERM || errno == EACCES)
                    {
                        answer = fly_ask (ASK_YN|ASK_WARN, "File `%s' is read-only, delete anyway?", NULL, pdel);
                        if (answer == 0) return;
                        if (answer == 1)
                        {
                            chmod (pdel, 700);
                            unlink (pdel);
                        }
                    }
                    else
                        fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                }
            }
            free (pdel);
        }
        l_chdir (pnl, NULL);
        if (prevfile != NULL)
            adjust_cursor (&local[pnl].dir, prevfile);
    }
    else
    {
#define NEWDELETE 0
#if NEWDELETE
        nsite = display.view[display.cursor];
        // check whether we have marked files
        mk = 0;
        for (i=0; i<RN_NF; i++)
            if (RN_FILE(i).flags & FILE_FLAG_MARKED)
                mk++;
        // ask whether to delete
        prevfile = NULL;
        if (mk)
        {
            if (RN_CURRENT != RN_NF-1)
                prevfile = strdup (RN_FILE(RN_CURRENT+1).name);
            else if (RN_CURRENT != 0)
                prevfile = strdup (RN_FILE(RN_CURRENT-1).name);
            answer = fly_ask (ASK_YN, MSG(M_DELETE_FILES), NULL, mk);
        }
        else
        {
            if (strcmp (LCURFILE(pnl).name, "..") == 0)
            {
                free (prevfile);
                return;
            }
            answer = fly_ask (ASK_YN, MSG(M_FTPS_DELETE), NULL, RN_CURFILE.name);
        }
        if (answer == 0)
        {
            free (prevdir);
            return;
        }

        // do the delete
        //rc = set_wd (nsite, RN_CURDIR.name, TRUE);
        //if (rc) return;

        noisy = TRUE;
        if (mk)
        {
            do_all_subdirs = FALSE;
            for (i=0; i<RN_NF; i++)
            {
                if (!(RN_FILE(i).flags & FILE_FLAG_MARKED)) continue;
                pdel = str_sjoin (RN_CURDIR.name, RN_FILE(i).name);

                if (RN_FILE(i).flags & FILE_FLAG_DIR)
                {
                    // directory
                    if ()
                    {
                        if (errno == ENOTEMPTY || errno == EEXIST)
                        {
                            // directory is not empty
                            if (!do_all_subdirs)
                            {
                                sprintf (buffer, "%s\n%s\n  %s  ", fl_str.yes, fl_str.no, MSG(M_ALL));
                                answer = fly_ask (ASK_WARN, MSG(M_NOT_EMPTY), buffer, RN_FILE(i).name);
                                if (answer == 0) {free (pdel); free (prevdir); return;}
                                if (answer == 3) do_all_subdirs = TRUE;
                                if (answer == 2) {free (pdel); continue;}
                            }
                            r_delete_subdir (pdel, &noisy);
                        }
                        else
                        {
                            // some other error besides NOTEMPTY
                            if (noisy)
                                fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                        }
                    }
                }
                else
                {
                    // regular file
                    rc = SendToServer (nsite, TRUE, &rsp, NULL, "DELE %s", RN_FILE(i).name);
                    if (rc) return;
                    if (rsp != 2)
                    {
                        if (noisy)
                            fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                    }
                }
                free (pdel);
            }
        }
        else
        {
            pdel = str_sjoin (RN_CURDIR.name, RCURFILE.name);
            if (RCURFILE.flags & FILE_FLAG_DIR)
            {
                if ()
                {
                    if (errno == ENOTEMPTY || errno == EEXIST)
                    {
                        if (fly_ask (ASK_YN|ASK_WARN, MSG(M_NOT_EMPTY), NULL, pdel))
                            r_delete_subdir (pdel, &noisy);
                        else
                        {
                            free (pdel);
                            return;
                        }
                    }
                    else
                    {
                        if (noisy)
                            fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                    }
                }
            }
            else
            {
                if (unlink (pdel))
                {
                    dmsg ("errno = %d\n", errno);
                    if (errno == EPERM || errno == EACCES)
                    {
                        answer = fly_ask (ASK_YN|ASK_WARN, "File `%s' is read-only, delete anyway?", NULL, pdel);
                        if (answer == 0) return;
                        if (answer == 1)
                        {
                            chmod (pdel, 700);
                            unlink (pdel);
                        }
                    }
                    else
                        fly_ask_ok (ASK_WARN, MSG(M_CANT_DELETE), pdel);
                }
            }
            free (pdel);
        }
        l_chdir (pnl, NULL);
        if (prevfile != NULL)
        {
            adjust_cursor (&local[pnl].dir, prevfile);
            free (prevfile);
        }

        ask_refresh (nsite, NULL);
#else
        if (!fly_ask (ASK_YN|ASK_WARN, MSG(M_REALLY_DELETE), NULL)) return;

        nsite = display.view[display.cursor];
        rc = set_wd (nsite, RN_CURDIR.name, TRUE);
        if (rc) return;

        // check whether we have marked files
        mk = FALSE;
        for (i=0; i<RN_NF; i++)
            if (RN_FILE(i).flags & FILE_FLAG_MARKED)
                mk = TRUE;

        rc = 0;
        buffer[0] = '\0';
        if (mk)
        {
            for (i=0; i<RN_NF; i++)
            {
                if (RN_FILE(i).flags & FILE_FLAG_MARKED)
                {
                    if (RN_FILE(i).flags & FILE_FLAG_REGULAR)
                    {
                        rc += SendToServer (nsite, TRUE, NULL, NULL, "DELE %s", RN_FILE(i).name);
                    }
                    else if (RN_FILE(i).flags & FILE_FLAG_DIR)
                    {
                        ;
                    }
                }
            }
        }
        else
        {
            if (RN_CURRENT != RN_NF-1)
                prevfile = RN_FILE(RN_CURRENT+1).name;
            else if (RN_CURRENT != 0)
                prevfile = RN_FILE(RN_CURRENT-1).name;
            if (RN_CURFILE.flags & FILE_FLAG_DIR)
                rc += SendToServer (nsite, TRUE, NULL, NULL, "RMD %s", RN_CURFILE.name);
            else
                rc += SendToServer (nsite, TRUE, NULL, NULL, "DELE %s", RN_CURFILE.name);
        }

        ask_refresh (nsite, NULL);
    }
#endif
}

/* ------------------------------------------------------------- */

void do_select_panel_contents (int key)
{
    int           chflags, i, n, sel, opp_site, v;
    unsigned long drivemap;
    char          *h, buffer[MAX_PATH], buf2[4], *p;
    url_t           u;

    v = display.cursor;
    dmsg ("1. display.view = %d, %d\n", display.view[0], display.view[1]);
    if (status.split_view)
    {
        if (v == V_LEFT)
        {
            h = "Select LEFT panel contents";
            chflags = CHOOSE_SHIFT_LEFT;
        }
        else
        {
            h = "Select RIGHT panel contents";
            chflags = CHOOSE_SHIFT_RIGHT;
        }
    }
    else
    {
        h = "Select panel contents";
        chflags = 0;
        v = display.cursor;
    }

    /* count number of things available to choose */
    ways = malloc (sizeof(way) * (MAX_SITE+26+2));
    n = 0;
    if (fl_opt.has_driveletters)
    {
        drivemap = query_drivemap ();
        for (i=0; i<26; i++)
        {
            if ((drivemap >> i) & 1)
            {
                ways[n].type = WT_DRIVE;
                ways[n].drive = i;
                n++;
            }
        }
    }
    else
    {
        ways[n].type = WT_LOCALPATH;
        ways[n].path = local[v].dir.name;
        n++; /* only one choice for local directory on Unix-like systems */
    }
    opp_site = display.view[1-v];
    for (i=0; i<MAX_SITE; i++)
    {
        if (site[i].set_up && i != opp_site)
        {
            ways[n].type = WT_FTP;
            ways[n].nsite = i;
            n++;
        }
    }
    ways[n++].type = WT_NEWFTP;
    ways[n++].type = WT_NEWPATH;

again:
    sel = fly_choose (h, chflags, &n, 0, drawline_goto, NULL);
    if (sel != -1)
    {
        switch (ways[sel].type)
        {
        case WT_LOCALPATH:
            display.view[v] = -1;
            break;

        case WT_DRIVE:
            if (change_drive (ways[sel].drive) || getcwd (buffer, sizeof (buffer)) == NULL)
            {
                fly_ask_ok (ASK_WARN, "  Cannot access drive %c  ", 'A' + ways[sel].drive);
                goto again;
            }
            strcpy (buf2, "A:/");
            buf2[0] += ways[sel].drive;
            p = str_sjoin (buf2, buffer);
            l_chdir (v, p);
            free (p);
            display.view[v] = -1;
            break;

        case WT_FTP:
            dmsg ("assigning %d\n", ways[sel].nsite);
            display.view[v] = ways[sel].nsite;
            break;

        case WT_NEWFTP:
            init_url (&u);
            u.userid[0] = '\0';
            u.password[0] = '\0';
            u.pathname[0] = '\0';
            if (Edit_Site (&u, TRUE) != PRESSED_ENTER) break;
            if (Login (-1, &u, v)) break;
            break;

        case WT_NEWPATH:
            // chdir...
            break;
        }
    }
    dmsg ("2. display.view = %d, %d\n", display.view[0], display.view[1]);

    free (ways);
}

/* ------------------------------------------------------------- */

void do_copy_local (int from, int to)
{
    char buffer[8192];

    if (!fly_ask (ASK_YN, " Copy %s \n from %s \n to   %s ?", NULL,
                  LCURFILE(from).name, local[from].dir.name, 
                  local[to].dir.name)) return;

    sprintf (buffer, "cp -Rp %s/%s %s",
             local[from].dir.name, LCURFILE(from).name,
             local[to].dir.name);

    put_message (" Copying, please wait... ");

    system (buffer);
    refresh (to);
}

/* ------------------------------------------------------------- */

void do_copy (int key)
{
    int from, to;

    from = display.view[display.cursor];
    to   = display.view[1-display.cursor];

    // find out what kind of transfer user wants
    if (from < 0 && to < 0)
    {
        // local-to-local copy
        do_copy_local (display.cursor, 1-display.cursor);
    }
    else if (from >= 0 && to < 0)
    {
        // download
        do_transfer (DOWN, from, 1-display.cursor);
    }
    else if (from < 0 && to >= 0)
    {
        // upload
        do_transfer (UP, display.cursor, to);
    }
    else
    {
        do_fxp (from, to);
    }
}

/* ------------------------------------------------------------- */

void do_move_local (int from, int to)
{
    char buffer[8192];

    if (!fly_ask (ASK_YN, " Move %s \n from %s \n to   %s ?", NULL,
                  LCURFILE(from).name, local[from].dir.name,
                  local[to].dir.name)) return;

    sprintf (buffer, "mv %s/%s %s",
             local[from].dir.name, LCURFILE(from).name,
             local[to].dir.name);

    put_message (" Moving, please wait... ");

    system (buffer);
    refresh (from);
    refresh (to);
}
/* ------------------------------------------------------------- */

void do_move (int key)
{
    int from, to;

    from = display.view[display.cursor];
    to   = display.view[1-display.cursor];

    // find out what kind of transfer user wants
    if (from < 0 && to < 0)
    {
        // local-to-local copy
        do_move_local (display.cursor, 1-display.cursor);
    }
    else if (from >= 0 && to < 0)
    {
        fly_ask_ok (ASK_WARN, "Not implemented yet!");
    }
    else if (from < 0 && to >= 0)
    {
        fly_ask_ok (ASK_WARN, "Not implemented yet!");
    }
    else
    {
        fly_ask_ok (ASK_WARN, "Not implemented yet!");
    }
}

/* ------------------------------------------------------------- */

void do_chdir (int key)
{
    char  buffer[MAX_PATH];
    int   nsite;

    if (display.view[display.cursor] < 0)
    {
        strcpy (buffer, local[display.cursor].dir.name);
        if (entryfield (MSG(M_ENTER_LOCAL_DIRECTORY), buffer, sizeof(buffer), 0) != PRESSED_ESC && buffer[0] != 0)
        {
            l_chdir (display.cursor, buffer);
        }
    }
    else
    {
        nsite = display.view[display.cursor];
        strcpy (buffer, RN_CURDIR.name);
        if (entryfield (MSG(M_ENTER_DIRECTORY), buffer, sizeof(buffer), 0) != PRESSED_ESC && buffer[0] != 0)
        {
            put_message (MSG(M_CHANGING_DIR));
            r_chdir (nsite, buffer, FALSE);
        }
    }
}

/* ------------------------------------------------------------- */

void do_rename (int key)
{
    char  buffer[MAX_PATH], *p1, *p2;
    int   nsite, pnl, rc, rsp;

    if (display.view[display.cursor] >= 0 && is_http_proxy(display.view[display.cursor]))
    {
        fly_ask_ok (0, MSG(M_HTTP_PROXY));
        return;
    }

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        if (!(LCURFILE(pnl).flags & FILE_FLAG_REGULAR || LCURFILE(pnl).flags & FILE_FLAG_DIR)) return;
        if (strcmp(LCURFILE(pnl).name, "..") == 0) return;
        strcpy (buffer, LCURFILE(pnl).name);
        if (entryfield (MSG(M_ENTER_NEWNAME), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
            buffer[0] == 0) return;
        p1 = str_sjoin (local[pnl].dir.name, LCURFILE(pnl).name);
        p2 = str_sjoin (local[pnl].dir.name, buffer);
        if (rename (p1, p2))
        {
            fly_ask_ok (ASK_WARN, MSG(M_RENAME_FAILED), p1, p2);
        }
        else
        {
            l_chdir (pnl, NULL);
            adjust_cursor (&local[display.cursor].dir, buffer);
        }
        free (p1);
        free (p2);
    }
    else
    {
        nsite = display.view[display.cursor];
        if (!(RN_CURFILE.flags & FILE_FLAG_REGULAR || RN_CURFILE.flags & FILE_FLAG_DIR)) return;
        if (strcmp (RN_CURFILE.name, "..") == 0) return;
        strcpy (buffer, RN_CURFILE.name);
        if (entryfield (MSG(M_ENTER_NEWNAME), buffer, sizeof(buffer), 0) == PRESSED_ESC || buffer[0] == 0) return;
        rc = set_wd (nsite, RN_CURDIR.name, TRUE);
        if (rc) return;
        SendToServer (nsite, TRUE, NULL, NULL, "RNFR %s", RN_CURFILE.name);
        SendToServer (nsite, TRUE, &rsp, NULL, "RNTO %s", buffer);
        if (rsp != 2)
            fly_ask_ok (ASK_WARN, MSG(M_RENAME_FAILED), RN_CURFILE.name, buffer);
        else
            ask_refresh (nsite, buffer);
        display.quicksearch[0] = '\0';
    }
}

/* ------------------------------------------------------------- */

void do_sort (int key)
{
    int   order, direction, pnl, nsite;
    char  *p;

    if (display.view[display.cursor] < 0)
    {
        order     = local[display.cursor].sortmode;
        direction = local[display.cursor].sortdirection;
    }
    else
    {
        order     = site[display.view[display.cursor]].sortmode;
        direction = site[display.view[display.cursor]].sortdirection;
    }

    switch (key)
    {
    case KEY_FM_SORT_NAME:    order = SORT_NAME; break;
    case KEY_FM_SORT_EXT:     order = SORT_EXT; break;
    case KEY_FM_SORT_TIME:    order = SORT_TIME; break;
    case KEY_FM_SORT_SIZE:    order = SORT_SIZE; break;
    case KEY_FM_SORT_UNSORT:  order = SORT_UNSORTED; break;
    case KEY_FM_SORT_REVERSE: direction = -direction; break;
    }

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        p = strdup (LCURFILE(pnl).name);
        local[pnl].sortmode = order;
        local[pnl].sortdirection = direction;
        sort_local_files (pnl);
        adjust_cursor (&local[pnl].dir, p);
        free (p);
    }
    else
    {
        nsite = display.view[display.cursor];
        p = strdup (RN_CURFILE.name);
        site[nsite].sortmode = order;
        site[nsite].sortdirection = direction;
        sort_remote_files (nsite);
        adjust_cursor (&RN_CURDIR, p);
        free (p);
    }
}

/* ------------------------------------------------------------- */

void do_mark (int key)
{
    int   pnl, nsite;
    char  *p;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        if (strcmp (LCURFILE(pnl).name, "..")) LCURFILE(pnl).flags ^= FILE_FLAG_MARKED;
        LCURFILENO(pnl) = min1 (LNFILES(pnl)-1, LCURFILENO(pnl)+1);
        LFIRSTITEM(pnl) = max1 (LFIRSTITEM(pnl), min1 (LFIRSTITEM(pnl)+1, LCURFILENO(pnl)-p_nlf()+1));
    }
    else
    {
        nsite = display.view[display.cursor];
        if (strcmp (RN_CURFILE.name, ".."))
        {
            RN_CURFILE.flags ^= FILE_FLAG_MARKED;
            if (RN_CURFILE.flags & FILE_FLAG_DIR)
            {
                p = str_sjoin (RN_CURDIR.name, RN_CURFILE.name);
                if (RN_CURFILE.flags & FILE_FLAG_MARKED)
                    set_marks (nsite, p);
                else
                    remove_marks (nsite, p);
                free (p);
            }
        }
        RN_CURRENT = min1 (RN_NF-1, RN_CURRENT+1);
        RN_FIRST   = max1 (RN_FIRST, min1 (RN_FIRST+1, RN_CURRENT-p_nlf()+1));
    }
}

/* ------------------------------------------------------------- */

void do_mark_by_mask (int key)
{
    int    i, pnl, nsite;
    char   buffer[MAX_PATH];

    strcpy (buffer, "*");
    if (entryfield (MSG(M_ENTER_SELECT_FILTER), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        for (i=0; i<LNFILES(pnl); i++)
        {
            if (fnmatch1 (buffer, LFILE(pnl,i).name, 0) && strcmp (LFILE(pnl,i).name, ".."))
            {
                if (key == KEY_FM_MASK_SELECT)
                    LFILE(pnl,i).flags |= FILE_FLAG_MARKED;
                else
                    LFILE(pnl,i).flags &= ~FILE_FLAG_MARKED;
            }
        }
    }
    else
    {
        nsite = display.view[display.cursor];
        for (i=0; i<RN_NF; i++)
        {
            if (fnmatch1 (buffer, RN_FILE(i).name, 0) && strcmp (RN_FILE(i).name, ".."))
            {
                if (key == KEY_FM_MASK_SELECT)
                    RN_FILE(i).flags |= FILE_FLAG_MARKED;
                else
                    RN_FILE(i).flags &= ~FILE_FLAG_MARKED;
            }
        }
    }
}

/* ------------------------------------------------------------- */

void do_mark_multiple (int key)
{
    int    i, pnl, nsite;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        for (i=0; i<LNFILES(pnl); i++)
        {
            if (strcmp (LFILE(pnl,i).name, "..") == 0) continue;
            switch (key)
            {
            case KEY_FM_SELECTALL:   LFILE(pnl,i).flags |= FILE_FLAG_MARKED;  break;
            case KEY_FM_DESELECTALL: LFILE(pnl,i).flags &= ~FILE_FLAG_MARKED; break;
            case KEY_FM_INVERT_SEL:  LFILE(pnl,i).flags ^= FILE_FLAG_MARKED;  break;
            }
        }
    }
    else
    {
        nsite = display.view[display.cursor];
        for (i=0; i<RN_NF; i++)
        {
            if (strcmp (RN_FILE(i).name, "..") == 0) continue;
            switch (key)
            {
            case KEY_FM_SELECTALL:   RN_FILE(i).flags |= FILE_FLAG_MARKED;  break;
            case KEY_FM_DESELECTALL: RN_FILE(i).flags &= ~FILE_FLAG_MARKED; break;
            case KEY_FM_INVERT_SEL:  RN_FILE(i).flags ^= FILE_FLAG_MARKED;  break;
            }
        }
    }
}

/* ------------------------------------------------------------- */

void do_export_marked (int key)
{
    int       i, j, marked_no, marked_dirs, pnl, nsite;
    int64_t   marked_size;
    char      buffer[MAX_PATH];
    FILE      *urlfile;
    url_t       u1;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
    }
    else
    {
        nsite = display.view[display.cursor];
        gather_marked (nsite, 0, &marked_no, &marked_dirs, &marked_size);
        if (marked_no == 0)
        {
            fly_ask_ok (0, MSG(M_NO_MARKED_FILES));
            return;
        }

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
            u1 = site[nsite].u;
            strcpy (u1.pathname, RN_CURDIR.name);
            fprintf (urlfile, "%s\n", compose_url (u1, RN_CURFILE.name, 1, 0));
        }
        else
        {
            u1 = site[nsite].u;
            for (i=0; i<site[nsite].ndir; i++)
            {
                strcpy (u1.pathname, site[nsite].dir[i].name);
                for (j=0; j<site[nsite].dir[i].nfiles; j++)
                {
                    if ((site[nsite].dir[i].files[j].flags & FILE_FLAG_REGULAR) &&
                        site[nsite].dir[i].files[j].flags & FILE_FLAG_MARKED)
                    {
                        fprintf (urlfile, "%s\n", compose_url (u1, site[nsite].dir[i].files[j].name, 1, 0));
                    }
                }
            }
        }
        fclose (urlfile);
        l_chdir (V_LEFT, NULL);
        l_chdir (V_RIGHT, NULL);
    }
}

/* ------------------------------------------------------------- */

void do_save_listing (int key)
{
    int      i, pnl, nsite;
    FILE     *fh;
    int64_t  ts;
    char     *url;
    char     buffer[MAX_PATH];

    buffer[0] = '\0';
    if (entryfield (MSG(M_ENTER_LISTING_FILENAME), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;

    fh = fopen (buffer, "w");
    if (fh == NULL)
    {
        fly_ask_ok (ASK_WARN, MSG(M_CANNOT_OPEN), buffer);
        return;
    }

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        fprintf (fh, "Directory: %s\n\n", local[pnl].dir.name);

        ts = 0;
        for (i=0; i<LNFILES(pnl); i++)
        {
            fprintf (fh, "%9s %s\n", printf64(LFILE(pnl,i).size), LFILE(pnl,i).name);
            //fputs (LFILE(pnl,i).rawtext, fh);
            if (LFILE(pnl,i).flags & FILE_FLAG_REGULAR) ts += LFILE(pnl,i).size;
        }
        fprintf (fh, MSG(M_LISTING_FOOTER), LNFILES(pnl), ts);
    }
    else
    {
        nsite = display.view[display.cursor];
        url = compose_url (site[nsite].u, "", 0, 0);
        fprintf (fh, MSG(M_LISTING_HEADER), site[nsite].u.hostname, RN_CURDIR.name);
        fprintf (fh, "URL: %s\n\n", url);

        ts = 0;
        for (i=0; i<RN_NF; i++)
        {
            fputs (RN_FILE(i).rawtext, fh);
            fputs ("\n", fh);
            if (RN_FILE(i).flags & FILE_FLAG_REGULAR) ts += RN_FILE(i).size;
        }
        fprintf (fh, MSG(M_LISTING_FOOTER), RN_NF, pretty64(ts));
    }
    fclose (fh);
    fly_ask_ok (0, MSG(M_LISTING_SAVED));
}

/* ------------------------------------------------------------- */

void do_chmod (int key)
{
    int  pnl, nsite;
    char buffer[1024], rights[80], buffer2[MAX_PATH];
    int  newrights, rsp, mk, i, rc;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
    }
    else
    {
        nsite = display.view[display.cursor];
        if (!site[nsite].features.chmod_works)
        {
            fly_ask_ok (0, MSG(M_NO_CHMOD), NULL);
        }
        else
        {
            strcpy (buffer2, RN_CURFILE.name);
            // check for selected files
            mk = 0;
            for (i=0; i<RN_NF; i++)
                if (RN_FILE(i).flags & FILE_FLAG_MARKED) mk++;

            rights[0] = '-';
            if (mk)
            {
                strcpy (rights+1, "rwxr-xr-x");
                sprintf (buffer, MSG(M_ACCESSRIGHTS1), mk);
            }
            else
            {
                strcpy (rights+1, perm_b2t (RN_CURFILE.rights));
                sprintf (buffer, MSG(M_ACCESSRIGHTS2), RN_CURFILE.name);
            }

            while (TRUE)
            {
                if (entryfield (buffer, rights+1, sizeof(rights)-1, 0) == PRESSED_ESC) return;
                newrights = perm_t2b (rights);
                if (newrights != 0) break;
                if (strcmp (rights, "----------") == 0) break;
                fly_ask_ok (ASK_WARN, MSG(M_WRONG_RIGHTS), rights+1);
                strcpy (rights+1, perm_b2t (RN_CURFILE.rights));
            }

            rc = set_wd (nsite, RN_CURDIR.name, TRUE);
            if (rc) return;

            if (mk)
            {
                for (i=0; i<RN_NF; i++)
                {
                    if (RN_FILE(i).flags & FILE_FLAG_MARKED)
                    {
                        rc = SendToServer (nsite, TRUE, &rsp, NULL, "SITE CHMOD %o %s", newrights & 0777, RN_FILE(i).name);
                        if (rc == 0 && rsp == 5)
                        {
                            fly_ask_ok (0, MSG(M_NO_CHMOD));
                            site[nsite].features.chmod_works = FALSE;
                            return;
                        }
                    }
                }
            }
            else
            {
                rc = SendToServer (nsite, TRUE, &rsp, NULL, "SITE CHMOD %o %s", newrights & 0777, RN_CURFILE.name);
                if (rc == 0 && rsp == 5)
                {
                    fly_ask_ok (0, MSG(M_NO_CHMOD));
                    site[nsite].features.chmod_works = FALSE;
                    return;
                }
            }
            ask_refresh (nsite, buffer2);
        }
    }
}

/* ------------------------------------------------------------- */

void do_reread (int key)
{
    refresh (display.cursor);
}

/* ------------------------------------------------------------- */

void do_go_root (int key)
{
    int  pnl, nsite;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        l_chdir (pnl, "/");
    }
    else
    {
        nsite = display.view[display.cursor];
        put_message (MSG(M_GOING_ROOT));
        r_chdir (nsite, "/", FALSE);
    }
}

/* ------------------------------------------------------------- */

void do_load_descriptions (int key)
{
    /*
     if (site[current_site].ndir == 1) break;
     if (RCURFILE.flags & FILE_FLAG_DIR &&
     fly_ask_ok (0, MSG (M_CANT_USE_DIR_AS_INDEX_FILE), RCURFILE.name))
     break;
     if (!fly_ask (ASK_YN, MSG (M_LOAD_DESCRIPTIONS), NULL, RCURFILE.name)) break;
     i = RCURFILENO;
     attach_descriptions (i);
     status.load_descriptions = TRUE;
     */
}

/* ------------------------------------------------------------- */

void do_walk (int key)
{
    int            walked, rc, oldmode;
    int            pnl, nsite;
    char           *p;

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
    }
    else
    {
        nsite = display.view[display.cursor];
        if (!(RN_CURFILE.flags & FILE_FLAG_DIR)) return;
        if (strcmp (RN_CURFILE.name, "..") == 0) return;
        walked = is_walked (nsite, site[nsite].cdir, RN_CURRENT);
        // make sure the tree is buffered
        if (!walked)
        {
            //if (options.ask_walktree && !fly_ask (ASK_YN, MSG(M_WALK_TREE), NULL, buffer)) return;
            oldmode = status.batch_mode;
            status.batch_mode = TRUE;
            rc = traverse_directory (nsite, RN_CURFILE.name, RN_CURFILE.flags & FILE_FLAG_MARKED);
            status.batch_mode = oldmode;
            if (rc) return;
            set_dir_size (nsite, site[nsite].cdir, RN_CURRENT);
            Bell (2);
        }
        else
        {
            set_dir_size (nsite, site[nsite].cdir, RN_CURRENT);
        }
        if (RN_CURFILE.flags & FILE_FLAG_MARKED)
        {
            p = str_sjoin (RN_CURDIR.name, RN_CURFILE.name);
            set_marks (nsite, p);
            free (p);
        }
    }
}

/* ------------------------------------------------------------- */

void do_select_font (int key)
{
    int    i, startpos, newfont;
    char   *p;

    if (fl_opt.is_x11)
        put_message (MSG(M_ENUMERATING_FONTS));
    if (F == NULL || nf <= 0)
        nf = fly_get_fontlist (&F, TRUE);
    if (nf <= 0)
    {
        fly_ask_ok (0, MSG(M_UNSUPPORTED), fl_opt.platform_name);
        return;
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
}

/* ------------------------------------------------------------- */

void do_exit (int key)
{
    int  i, asked = FALSE;

    for (i=0; i<MAX_SITE; i++)
    {
        if (!is_http_proxy(i) && site[i].connected)
        {
            // we're connected
            if (options.ask_logoff)
            {
                // ask about logoff if not disabled
                if (ask_logoff(i)) return;
                asked = TRUE;
            }
        }
        Logoff (i);
    }
    if (!asked && options.ask_exit)
    {
        if (!fly_ask (ASK_YN, MSG(M_REALLY_EXIT), NULL)) return;
    }
    terminate ();
    exit (0);
}

/* ------------------------------------------------------------- */

void do_edit_ini (int key)
{
    char buffer[MAX_PATH*3];

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
}

/* ------------------------------------------------------------- */

void do_logoff (int key)
{
    int nsite, nconn = MAX_SITE;

    nsite = display.view[display.cursor];
    if (nsite < 0)
    {
        nsite = fly_choose ("What connection to close?", 0, &nconn, 0, drawline_conn, NULL);
    }

    if (nsite >= 0 && site[nsite].set_up)
    {
        if (is_http_proxy(nsite))
            fly_ask_ok (0, MSG(M_HTTP_PROXY));
        else
            Logoff (nsite);
    }
}

/* ------------------------------------------------------------- */

#define XTERM "xterm -ls -fn ssck-14 -bg rgb:a0/a0/a0 -geometry 82x30+14+25"

void do_enter_dir (int key)
{
    int   pnl, nsite;
    char  *p, buffer[MAX_PATH*3], buffer2[8192];

    if (display.view[display.cursor] < 0)
    {
        pnl = display.cursor;
        if (LCURFILE(pnl).flags & FILE_FLAG_DIR)
        {
            p = strdup (LCURFILE(pnl).name);
            l_chdir (pnl, p);
            free (p);
        }
        else if ((LCURFILE(pnl).flags & FILE_FLAG_REGULAR) &&
                 (LCURFILE(pnl).rights & (S_IXUSR|S_IXGRP|S_IXOTH)))
        {
            strcpy (buffer, local[pnl].dir.name);
            str_cats (buffer, LCURFILE(pnl).name);
            strcat (buffer, " ");
            if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;
            sprintf (buffer2, "%s -e sh -c \"cd %s; %s; echo press enter to continue; read x\"", XTERM, local[pnl].dir.name, buffer);
            fly_launch (buffer2, TRUE, TRUE);
            refresh (pnl);
        }
    }
    else
    {
        nsite = display.view[display.cursor];
        if (!(RN_CURFILE.flags & (FILE_FLAG_DIR|FILE_FLAG_LINK))) return;
        if (site[nsite].system_type == SYS_MVS_NATIVE)
        {
            dmsg ("MVS: entering %s from %s\n", RN_CURFILE.name, site[nsite].u.pathname);
            if (strcmp (RN_CURFILE.name, "..") == 0)
            {
                // check for going to the "top"
                if (strchr (site[nsite].u.pathname, '.') == NULL)
                {
                    dmsg ("MVS: at the top\n");
                    buffer[0] = '\0';
                    if (entryfield (MSG(M_ENTER_DIRECTORY), buffer, sizeof(buffer), 0) == PRESSED_ESC ||
                        buffer[0] == 0) return;
                    put_message (MSG(M_CHANGING_DIR));
                    r_chdir (nsite, "..", FALSE);
                    r_chdir (nsite, buffer, FALSE);
                }
                else
                {
                    r_chdir (nsite, RN_CURFILE.name, RN_CURFILE.flags & FILE_FLAG_MARKED);
                }
            }
            else
            {
                r_chdir (nsite, RN_CURFILE.name, RN_CURFILE.flags & FILE_FLAG_MARKED);
            }
        }
        else
        {
            r_chdir (nsite, RN_CURFILE.name, RN_CURFILE.flags & FILE_FLAG_MARKED);
        }
    }
    display.quicksearch[0] = '\0';
}

/* ------------------------------------------------------------- */

void do_transfer (int where, int from, int to)
{
    int      i, mk, nsite, pnl, rc, oldmode, walked;
    int      m_no, m_dirs;
    int64_t  m_size;

    if (where == DOWN)
    {
        nsite = from;
        pnl   = to;

        // check whether we have marked files
        gather_marked (nsite, FALSE, &m_no, &m_dirs, &m_size);
        if (m_no == 0) RN_CURFILE.flags |= FILE_FLAG_MARKED;

        // now we must make sure that subdirectories of marked directories are
        // fully buffered
        for (i=0; i<RN_NF; i++)
        {
            if (RN_FILE(i).flags & FILE_FLAG_MARKED &&
                RN_FILE(i).flags & FILE_FLAG_DIR)
            {
                walked = is_walked (nsite, site[nsite].cdir, i);
                dmsg ("walked = %d for %s / %s\n",
                      walked, RN_CURDIR.name, RN_FILE(i).name);
                if (!walked)
                {
                    oldmode = status.batch_mode;
                    status.batch_mode = TRUE;
                    rc = traverse_directory (nsite, RN_FILE(i).name, TRUE);
                    if (rc == 0) set_dir_size (nsite, site[nsite].cdir, i);
                    //set_marks (nsite, i);
                    status.batch_mode = oldmode;
                }
            }
        }

        // do actual transfer
        do_download_marked (nsite, pnl);
    }
    else
    {
        nsite = to;
        pnl = from;

        // check whether we have marked files
        mk = FALSE;
        for (i=0; i<LNFILES(pnl); i++)
        {
            if (LFILE(pnl,i).flags & FILE_FLAG_MARKED)
            {
                mk = TRUE;
                break;
            }
        }
        if (!mk) LCURFILE(pnl).flags |= FILE_FLAG_MARKED;

        do_upload_marked (pnl, nsite);
    }
}

/* ------------------------------------------------------------- */

void do_download_marked (int nsite, int pnl)
{
    int         i, j, l, nt, nta, keep_tree=TRUE;
    trans_item  *T;
    char        *subdir;

    history_add (&site[nsite].u);

    // compose transfer list
    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    for (i=0; i<site[nsite].ndir; i++)
    {
        dmsg ("checking directory %s\n", site[nsite].dir[i].name);
        // skip files outside current tree
        l = strlen (RN_CURDIR.name);
        if (keep_tree && strncmp (site[nsite].dir[i].name, RN_CURDIR.name, l) != 0) continue;
        if (RN_CURDIR.name[l-1] == '/') l--;
        subdir = str_sjoin (local[pnl].dir.name, site[nsite].dir[i].name+l);
        str_strip (subdir, "/");
        dmsg ("accepted; subdir = %s\n", subdir);
        for (j=0; j<site[nsite].dir[i].nfiles; j++)
        {
            dmsg ("FILE: %s (%x)\n", site[nsite].dir[i].files[j].name, site[nsite].dir[i].files[j].flags);
            if (site[nsite].dir[i].files[j].flags & FILE_FLAG_REGULAR &&
                site[nsite].dir[i].files[j].flags & FILE_FLAG_MARKED)
            {
                T[nt].r_dir  = strdup (site[nsite].dir[i].name);
                T[nt].r_name = strdup (site[nsite].dir[i].files[j].name);
                T[nt].l_dir  = strdup (subdir);
                T[nt].l_name = strdup (site[nsite].dir[i].files[j].name);
                T[nt].mtime  = site[nsite].dir[i].files[j].mtime;
                T[nt].size   = site[nsite].dir[i].files[j].size;
                T[nt].perm   = site[nsite].dir[i].files[j].rights;
                T[nt].description = site[nsite].dir[i].files[j].desc;
                T[nt].f = &(site[nsite].dir[i].files[j]);
                T[nt].flags = 0;
                nt++;
                if (nt == nta)
                {
                    nta *= 2;
                    T = realloc (T, nta * sizeof (trans_item));
                }
                dmsg ("accepted\n");
            }
        }
        free (subdir);
    }

    transfer (DOWN, nsite, -1, T, nt, TRUE);

    free (T);

    // remove marks
    remove_marks (nsite, RN_CURDIR.name);
}

/* ------------------------------------------------------------- */

void do_upload_marked (int pnl, int nsite)
{
    char        *p;
    int         i, j, nt, nta, to_cdir;
    trans_item  *T;

    to_cdir = site[nsite].cdir;

    // first we need to build cached version of local directory
    // structure if some local directories are marked
    for (i=0; i<LNFILES(pnl); i++)
    {
        if (LFILE(pnl,i).flags & FILE_FLAG_DIR &&
            LFILE(pnl,i).flags & FILE_FLAG_MARKED)
        {
            p = str_sjoin (local[pnl].dir.name, LFILE(pnl,i).name);
            lcache_scandir (p);
            free (p);
        }
    }

    // then we can built a list of files which have to be uploaded
    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    for (i=0; i<LNFILES(pnl); i++)
    {
        if (!(LFILE(pnl,i).flags & FILE_FLAG_MARKED)) continue;

        if (LFILE(pnl,i).flags & FILE_FLAG_DIR)
        {
            p = str_sjoin (local[pnl].dir.name, LFILE(pnl,i).name);
            //lcache_scandir (p);
            free (p);
        }
        else if (LFILE(pnl,i).flags & FILE_FLAG_REGULAR)
        {
            T[nt].r_dir  = strdup (RN_CURDIR.name);
            T[nt].r_name = strdup (LFILE(pnl,i).name);
            T[nt].l_dir  = strdup (local[pnl].dir.name);
            T[nt].l_name = strdup (LFILE(pnl,i).name);
            T[nt].mtime  = LFILE(pnl,i).mtime;
            T[nt].size   = LFILE(pnl,i).size;
            T[nt].perm   = LFILE(pnl,i).rights;
            T[nt].description = LFILE(pnl,i).desc;
            T[nt].f = &(LFILE(pnl,i));
            T[nt].flags = 0;
            nt++;
            if (nt == nta)
            {
                nta *= 2;
                T = realloc (T, nta * sizeof (trans_item));
            }
        }
    }

    transfer (UP, nsite, -1, T, nt, TRUE);

    lcache_free ();
    ask_refresh (nsite, NULL);

    // invalidate subdirectories of site[nsite].dir[cdir]
    p = strdup (site[nsite].dir[to_cdir].name);
again:
    for (i=0; i<site[nsite].ndir; i++)
    {
        if (i != to_cdir && str_headcmp (site[nsite].dir[i].name, p) == 0)
        {
            dmsg ("freeing %s\n", site[nsite].dir[i].name);
            free (site[nsite].dir[i].files);
            for (j=i; j<site[nsite].ndir-1; j++)
                site[nsite].dir[j] = site[nsite].dir[j+1];
            if (site[nsite].ndir > 1) site[nsite].ndir--;
            goto again;
        }
    }
    free (p);
}

/* ------------------------------------------------------------- */

void do_fxp (int nsite, int ns2)
{
    int        i, j, rc, oldmode, walked;
    int        m_no, m_dirs;
    int64_t    m_size;
    int        nt, nta, l, keep_tree = TRUE;
    trans_item *T;
    char       *subdir, *p;
    int        from_cdir, to_cdir;

    if (display.view[display.cursor] >= 0 && is_http_proxy(display.view[display.cursor]))
    {
        fly_ask_ok (0, MSG(M_HTTP_PROXY));
        return;
    }

    from_cdir = site[nsite].cdir;
    to_cdir = site[ns2].cdir;

    // check whether we have marked files
    gather_marked (nsite, FALSE, &m_no, &m_dirs, &m_size);
    if (m_no == 0)
    {
        RN_CURFILE.flags |= FILE_FLAG_MARKED;
        p = str_sjoin (RN_CURDIR.name, RN_CURFILE.name);
        set_marks (nsite, p);
        free (p);
    }

    // now we must make sure that subdirectories of marked directories are
    // fully buffered
    for (i=0; i<RN_NF; i++)
    {
        if (RN_FILE(i).flags & FILE_FLAG_MARKED &&
            RN_FILE(i).flags & FILE_FLAG_DIR)
        {
            walked = is_walked (nsite, site[nsite].cdir, i);
            dmsg ("walked = %d for %s / %s\n",
                  walked, RN_CURDIR.name, RN_FILE(i).name);
            if (!walked)
            {
                oldmode = status.batch_mode;
                status.batch_mode = TRUE;
                rc = traverse_directory (nsite, RN_FILE(i).name, TRUE);
                if (rc == 0) set_dir_size (nsite, site[nsite].cdir, i);
                //set_marks (nsite, i);
                status.batch_mode = oldmode;
            }
        }
    }

    // do actual transfer
    history_add (&site[nsite].u);

    // compose transfer list
    nt = 0;
    nta = 64;
    T = malloc (nta * sizeof (trans_item));

    for (i=0; i<site[nsite].ndir; i++)
    {
        // skip files outside current tree
        l = strlen (RN_CURDIR.name);
        if (keep_tree && strncmp (site[nsite].dir[i].name, RN_CURDIR.name, l) != 0) continue;
        if (RN_CURDIR.name[l-1] == '/') l--;
        subdir = str_sjoin (site[ns2].dir[site[ns2].cdir].name, site[nsite].dir[i].name+l);
        str_strip (subdir, "/");
        for (j=0; j<site[nsite].dir[i].nfiles; j++)
        {
            if (site[nsite].dir[i].files[j].flags & FILE_FLAG_REGULAR &&
                site[nsite].dir[i].files[j].flags & FILE_FLAG_MARKED)
            {
                T[nt].r_dir  = strdup (site[nsite].dir[i].name);
                T[nt].r_name = strdup (site[nsite].dir[i].files[j].name);
                T[nt].l_dir  = strdup (subdir);
                T[nt].l_name = strdup (site[nsite].dir[i].files[j].name);
                T[nt].mtime  = site[nsite].dir[i].files[j].mtime;
                T[nt].size   = site[nsite].dir[i].files[j].size;
                T[nt].perm   = site[nsite].dir[i].files[j].rights;
                T[nt].description = site[nsite].dir[i].files[j].desc;
                T[nt].f = &(site[nsite].dir[i].files[j]);
                T[nt].flags = 0;
                nt++;
                if (nt == nta)
                {
                    nta *= 2;
                    T = realloc (T, nta * sizeof (trans_item));
                }
            }
        }
        free (subdir);
    }

    transfer (FXP, nsite, ns2, T, nt, TRUE);

    free (T);

    // remove marks
    remove_marks (nsite, RN_CURDIR.name);
    ask_refresh (ns2, NULL);

    // invalidate subdirectories of site[ns2].dir[cdir]
    p = strdup (site[ns2].dir[to_cdir].name);
again:
    for (i=0; i<site[ns2].ndir; i++)
    {
        if (i != to_cdir && str_headcmp (site[ns2].dir[i].name, p) == 0)
        {
            dmsg ("freeing %d: %s\n", i, site[ns2].dir[i].name);
            free (site[ns2].dir[i].files);
            for (j=i; j<site[ns2].ndir-1; j++)
                site[ns2].dir[j] = site[ns2].dir[j+1];
            if (site[ns2].ndir > 1) site[ns2].ndir--;
            goto again;
        }
    }
    free (p);
}

/* ------------------------------------------------------------- */

void do_quote (int key)
{
    int   nsite;
    char  buffer[1024];

    if (display.view[display.cursor] < 0) return;
    nsite = display.view[display.cursor];
    if (is_http_proxy(nsite))
    {
        fly_ask_ok (0, MSG(M_HTTP_PROXY));
        return;
    }

    *buffer = 0;
    if (entryfield (MSG(M_ENTER_VERBATIM_COMMAND), buffer, sizeof(buffer), 0) != PRESSED_ESC &&
        buffer[0] != 0)
    {
        put_message (MSG(M_SENDING_VERBATIM_COMMAND));
        SendToServer (nsite, TRUE, NULL, NULL, buffer);
        do_show_cc (KEY_GEN_SWITCH_TO_CC);
    }
}

/* ------------------------------------------------------------- */

void do_scroll (int key)
{
    int nsite;

    display.quicksearch[0] = '\0';
    if (display.view[display.cursor] < 0)
    {
        fly_scroll_it (key,
                       &(local[display.cursor].dir.first),
                       &(local[display.cursor].dir.current),
                       local[display.cursor].dir.nfiles,
                       p_nlf());
    }
    else
    {
        nsite = display.view[display.cursor];
        fly_scroll_it (key, &(RN_FIRST), &(RN_CURRENT), RN_NF, p_nlf());
    }
    fix_screen_pos (display.cursor, p_nlf());
    if (key == KEY_HOME) display.rshift = 0;
}

/* ------------------------------------------------------------- */

void do_launch_wget (int key)
{
    /*
    int      i, j, marked_no, marked_dirs;
    int64_t  marked_size;
    char     wget_input[MAX_PATH], buffer[4096];
    FILE     *urlfile;
    url_t      u1;

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
        u1 = site[current_site].u;
        strcpy (u1.pathname, RCURDIR.name);
        fprintf (urlfile, "%s\n", compose_url (u1, RCURFILE.name, 1, 0));
    }
    else
    {
        u1 = site[current_site].u;
        for (i=0; i<site[current_site].ndir; i++)
        {
            strcpy (u1.pathname, site[current_site].dir[i].name);
            for (j=0; j<site[current_site].dir[i].nfiles; j++)
            {
                if ((site[current_site].dir[i].files[j].flags & FILE_FLAG_REGULAR) &&
                    site[current_site].dir[i].files[j].flags & FILE_FLAG_MARKED)
                {
                    fprintf (urlfile, "%s\n", compose_url (u1, site[current_site].dir[i].files[j].name, 1, 0));
                }
            }
        }
    }
    fclose (urlfile);
    chmod (wget_input, 0600);

    sprintf (buffer, options.wget_command, wget_input);
    if (entryfield (MSG(M_CONFIRM_COMMAND), buffer, sizeof(buffer), 0) == PRESSED_ESC) return;
    fly_launch (buffer, FALSE, FALSE);

    // remove marks
    if (marked_no != 0)
        for (i=0; i<site[current_site].ndir; i++)
            for (j=0; j<site[current_site].dir[i].nfiles; j++)
            site[current_site].dir[i].files[j].flags &= ~FILE_FLAG_MARKED;
            */
}

/* ------------------------------------------------------------- */

void do_ftps_find (int key)
{
    url_t  u;
    int  nsite;
    char *p;

    init_url (&u);
    p = "";
    if (display.view[display.cursor] >= 0)
    {
        nsite = display.view[display.cursor];
        if (RN_CURFILE.flags & FILE_FLAG_REGULAR)
            p = RN_CURFILE.name;
    }
    if (ftpsearch_find (p, &u) < 0) return;
    if (Edit_Site (&u, FALSE) != PRESSED_ENTER) return;
    if (Login (-1, &u, display.cursor)) return;
    if (options.login_bell) Bell (3);
}

/* ------------------------------------------------------------- */

void do_ftps_recall (int key)
{
    url_t u;

    init_url (&u);
    if (ftpsearch_recall (&u) < 0) return;
    if (Edit_Site (&u, TRUE) != PRESSED_ENTER) return;
    if (Login (-1, &u, display.cursor)) return;
    if (options.login_bell) Bell (3);
}

/* ------------------------------------------------------------- */

void do_binascii (int key)
{
    int nsite;

    status.binary = !status.binary;
    for (nsite=0; nsite<MAX_SITE; nsite++)
    {
        if (site[nsite].set_up && site[nsite].connected &&
            !is_http_proxy(nsite))
            SetTransferMode (nsite);
    }
}

