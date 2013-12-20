#include <fly/fly.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

#include "nftp.h"
#include "keys.h"
#include "search.h"
#include "auxprogs.h"

#define VID_NORMAL  (_BackBlack + _White)
#define VID_REVERSE (_BackWhite + _Black)
#define VID_BRIGHT  (_BackBlack + _High + _White)

static void def_key_assignments (void);
static void key_assignments (void);
//static void color_assignments (void);

static char sect_network[]      = "network";
static char sect_options[]      = "options";
static char sect_registration[] = "registration";
static char sect_fire[]         = "firewalling";
static char sect_history[]      = "history";
static char sect_layout[]       = "layout";
static char sect_ftpsearch[]    = "ftpsearch";
static char sect_clr[]          = "colours";
static char sect_psw[]          = "passwords";

static struct
{
    char  *sect;
    char  *name;
    int   def;
    int   *value;
}
boptions[] =
{
    {sect_options, "debug", FALSE, &options.debug},
    {sect_options, "login-bell", TRUE, &options.login_bell},
    {sect_options, "transfer-pause", TRUE, &options.transfer_pause},
    {sect_options, "log-transfers", TRUE, &options.log_trans},
    {sect_options, "save-bookmark-at-logoff", FALSE, &options.save_marks},
    {sect_options, "auto-switch-to-control", FALSE, &options.autocontrol},
    {sect_options, "bmk-show-server-info", TRUE, &options.show_serverinfo},
    {sect_options, "dns-lookup", FALSE, &options.dns_lookup},
    {sect_options, "lowercase-names", FALSE, &options.lowercase_names},
    {sect_options, "english-menu", FALSE, &options.english_menu},
    {sect_options, "enable-passworded-wget", FALSE, &options.passworded_wget},
    {sect_options, "lynx-navigation-keys", FALSE, &options.lynx_keys},
    {sect_options, "monochrome", FALSE, &options.monochrome},
    {sect_options, "mouse-support", TRUE, &options.mouse},
    {sect_options, "autopaste-url", TRUE, &options.autopaste},
    {sect_options, "assume-slow-link", FALSE, &options.slowlink},
    {sect_options, "save-window-position", TRUE, &options.keep_winpos},
    {sect_options, "save-window-size", TRUE, &options.keep_winsize},
    {sect_options, "hotmouse-in-menu", TRUE, &options.hotmouse_menu},
    {sect_options, "show-timestamp-in-cc", TRUE, &options.show_cc_time},
    {sect_options, "use_termcap1", TRUE, &options.use_termcap1},
    {sect_options, "show-dotted", TRUE, &options.show_dotfiles},
    {sect_options, "as400-server-uses-put", FALSE, &options.as400_put},
    {sect_options, "ask-before-walking-dir-tree", TRUE, &options.ask_walktree},
    {sect_options, "preserve-timestamp-on-download", TRUE, &options.preserve_timestamp_download},
    {sect_options, "preserve-timestamp-on-upload", TRUE, &options.preserve_timestamp_upload},
    {sect_options, "preserve-permissions-on-download", TRUE, &options.preserve_permissions_download},
    {sect_options, "preserve-permissions-on-upload", TRUE, &options.preserve_permissions_upload},
    {sect_options, "VMS-strip-version-number", TRUE, &options.vms_remove_version},
    {sect_options, "flush-HTTP-cache", FALSE, &options.flush_http_cache},
    {sect_options, "try-FTP-extensions", FALSE, &options.try_ftpext},
    {sect_options, "confirm-quit", TRUE, &options.ask_exit},
    {sect_options, "relaxed-mirroring", FALSE, &options.relaxed_mirror},
    //{sect_options, "query-bfs-attributes-support", FALSE, &options.query_bfsattrs},
    {sect_options, "use-bfs-files", FALSE, &options.use_bfs_files},
    {sect_options, "confirm-reread", TRUE, &options.ask_reread},
    {sect_options, "confirm-logoff", TRUE, &options.ask_logoff},
    {sect_options, "os390-hide-non-datasets", TRUE, &options.os390_hide},
    {sect_options, "guess-cyrillic-charsets", TRUE, &options.guess_cyrillic},
    {sect_options, "log-control-connection", FALSE, &options.log_cc},
    {sect_options, "fxp-always-skips", FALSE, &options.fxp_skips},
    {sect_options, "enter-subdir-after-create", FALSE, &options.create_enters},

/*
; whether to assume remote file times to be in UTC. most FTP servers supply
; times for files in the listings in UTC. according general rules they should
; be converted to local time before being displaying to user, but this has
; rather unexpected effect: other FTP clients will show different times
; for remote files! historically NFTP assumed remote file times to be in
; abstract 'local' time and did no conversion. this option allows to change
; this behaviour. if you are unsure please don't change it. I hope to write
; more or less clear explanation of this issue in a special article later
; at some point (and totally revise/rewrite corresponding routines in NFTP
; for more consistent handling of this issue). default: no (for historical reasons)
;
;remote-is-UTC=no
    {sect_options, "remote-is-UTC", FALSE, &options.use_UTC},

*/

    {sect_fire, "fwbug1", FALSE, &options.fwbug1},

    {sect_ftpsearch, "show-servers", TRUE, &options.ftps_show_servers},

    {sect_history, "active", TRUE, &options.history_active},
    {sect_history, "record-everything", TRUE, &options.history_everything},

    {sect_layout, "menu-onscreen", TRUE, &fl_opt.menu_onscreen},
    {sect_layout, "hide-dates-too", FALSE, &options.hide_dates_too},
    {sect_layout, "full-frames", FALSE, &options.full_frames},
    {sect_layout, "shadows", TRUE, &fl_opt.shadows},
    {sect_layout, "GUI-controls", TRUE, &fl_opt.use_gui_controls},
    {sect_layout, "show-server-name", TRUE, &options.show_servername},
    {sect_layout, "show-panel-headings", TRUE, &options.show_panel_headings},
};

static struct
{
    char  *sect;
    char  *name;
    int   def;
    int   *value;
}
ioptions[] =
{
    {sect_options, "start-prompt", 0, &options.start_prompt},
    {sect_options, "menu-open-submenu", 0, &options.menu_open_sub},
    {sect_options, "pseudographics", 0, &options.pseudographics},
    {sect_options, "default-sort", SORT_NAME, &options.defaultsort},
    {sect_options, "default-local-sort", SORT_NAME, &options.default_localsort},
    {sect_options, "default-dir-mode", MODE_PARSED, &options.defaultdirmode},
    {sect_options, "login-retry", 60*5, &options.retry_pause},
    {sect_options, "desc-size-warn", 50, &options.desc_size_limit},
    {sect_options, "desc-time-warn", 25, &options.desc_time_limit},
    {sect_options, "description-indentation", 4, &options.desc_indent},
    {sect_options, "as400-date-format", 3, &options.as400_date_fmt},
    {sect_options, "stalled-interval", 30, &options.stalled_interval},
    {sect_options, "batch-transfer-limit", 1, &options.batch_limit},
    {sect_options, "auto-refresh", 0, &options.auto_refresh},

    //{sect_psw, "encryption-type", 0, &options.psw_enctype},

    //{sect_layout, "top-status-line", 0, &options.top_status_style},
    //{sect_layout, "bottom-status-line", 1, &options.bottom_status_style},

    {sect_network, "default-port", 21, &options.defaultport},
    {sect_network, "network-timeout", 60*3, &options.data_timeout},
    {sect_network, "cc-timeout", 60*3, &options.cc_timeout},

    {sect_fire, "firewall-type", 0, &options.firewall_type},
    {sect_fire, "firewall-port", 21, &options.fire_port},

    {sect_history, "entries-watermark", 5000, &options.history_entries_watermark},
    {sect_history, "sites-watermark", 5000, &options.history_sites_watermark},
    {sect_history, "persite-watermark", 500, &options.history_persite_watermark},
};

static struct
{
    char  *sect;
    char  *name;
    char  *def;
    char  **value;
}
soptions[] =
{
    {sect_network, "anonymous-name", "anonymous", &options.anon_name},
    {sect_network, "anonymous-password", "nftp@", &options.anon_pass},

    {sect_options, "user-agent", NULL, &options.user_agent},
    {sect_options, "launch-wget", "wget -a wget.log -i %s", &options.wget_command},
    {sect_options, "description-file", NULL, &options.descfile},
    {sect_options, "default-download-path", NULL, &options.download_path},
    {sect_options, "version", "1.00", &options.version},
    {sect_options, "transfer-complete-action", "bell", &options.transfer_complete_action},
    {sect_options, "custom-selection-mask", "*.zip", &options.selection_mask},

    {sect_fire, "firewall-host", "", &options.fire_server},
    {sect_fire, "firewall-login", "", &options.fire_login},
    {sect_fire, "firewall-password", "", &options.fire_passwd},

    {sect_history, "history-file", NULL, &options.history_file},

    {sect_psw, "passwords-file", NULL, &options.psw_file}
};

static struct
{
    char           *sect;
    char           *name;
    unsigned char  def;
    unsigned char  *value;
}
xoptions[] =
{
    {sect_clr, "background",       _BackBlack + _White,        &options.attr_background},
    {sect_clr, "status2",          _BackWhite + _Black,        &options.attr_status2},
    {sect_clr, "help",             _BackWhite + _Black,        &options.attr_help},

    {sect_clr, "dialog-box-text",       255, &fl_clr.dbox_back},
    {sect_clr, "dialog-box-text-warn",  255, &fl_clr.dbox_back_warn},
    {sect_clr, "dialog-box-selected",   255, &fl_clr.dbox_sel},
    {sect_clr, "dialog-box-entryfield", 255, &fl_clr.entryfield},

    {sect_clr, "cc-header",   _BackWhite + _Black,  &options.attr_cntr_header},
    {sect_clr, "cc-response", _BackBlack + _White, &options.attr_cntr_resp},
    {sect_clr, "cc-command",  _BackBlack + _High + _Green,  &options.attr_cntr_cmd},
    {sect_clr, "cc-comment",  _BackBlack + _High + _Red,   &options.attr_cntr_comment},

    {sect_clr, "bookmark-background",       _BackWhite + _Black, &options.attr_bmrk_back},
    {sect_clr, "bookmark-pointer",          _BackCyan + _Black,  &options.attr_bmrk_pointer},
    {sect_clr, "bookmark-hostpath",         _BackWhite + _Red,   &options.attr_bmrk_hostpath},
    {sect_clr, "bookmark-hostpath-pointer", _BackCyan + _Red,    &options.attr_bmrk_hostpath_pointer},

    {sect_clr, "TP-column-header",              _BackBlue + _High + _White, &options.attr_tp_header},
    {sect_clr, "TP-file",                       _BackBlue + _White,         &options.attr_tp_file},
    {sect_clr, "TP-directory",                  _BackBlue + _White,         &options.attr_tp_dir},
    {sect_clr, "TP-file-marked",                _BackBlue + _High + _White, &options.attr_tp_file_m},
    {sect_clr, "TP-directory-marked",           _BackBlue + _High + _White, &options.attr_tp_dir__m},
    {sect_clr, "TP-file-pointer",               _BackCyan + _Black,         &options.attr_tp_file_p},
    {sect_clr, "TP-directory-pointer",          _BackCyan + _Black,         &options.attr_tp_dir__p},
    {sect_clr, "TP-file-marked-pointer",        _BackCyan + _High + _White, &options.attr_tp_file_mp},
    {sect_clr, "TP-directory-marked-pointer",   _BackCyan + _High + _White, &options.attr_tp_dir__mp},

    {sect_clr, "SP-column-header",              _BackBlue + _High + _White, &options.attr_sp_header},
    {sect_clr, "SP-file",                       _BackBlue + _White,         &options.attr_sp_file},
    {sect_clr, "SP-directory",                  _BackBlue + _White,         &options.attr_sp_dir},
    {sect_clr, "SP-description",                _BackBlue + _White,         &options.attr_sp_desc},
    {sect_clr, "SP-file-marked",                _BackBlue + _High + _White, &options.attr_sp_file_m},
    {sect_clr, "SP-directory-marked",           _BackBlue + _High + _White, &options.attr_sp_dir__m},
    {sect_clr, "SP-description-marked",         _BackBlue + _High + _White, &options.attr_sp_desc_m},
    {sect_clr, "SP-file-pointer",               _BackCyan + _Black,         &options.attr_sp_file_p},
    {sect_clr, "SP-directory-pointer",          _BackCyan + _Black,         &options.attr_sp_dir__p},
    {sect_clr, "SP-description-pointer",        _BackCyan + _Black,         &options.attr_sp_desc_p},
    {sect_clr, "SP-file-marked-pointer",        _BackCyan + _High + _White, &options.attr_sp_file_mp},
    {sect_clr, "SP-directory-marked-pointer",   _BackCyan + _High + _White, &options.attr_sp_dir__mp},
    {sect_clr, "SP-description-marked-pointer", _BackCyan + _High + _White, &options.attr_sp_desc_mp},

    {sect_clr, "viewer-text",      255, &fl_clr.viewer_text},
    {sect_clr, "viewer-header",    255, &fl_clr.viewer_header},
    {sect_clr, "viewer-foundtext", 255, &fl_clr.viewer_found},

    {sect_clr, "menu-main",            255, &fl_clr.menu_main},
    {sect_clr, "menu-main-disabled",   255, &fl_clr.menu_main_disabled},
    {sect_clr, "menu-main-hotkey",     255, &fl_clr.menu_main_hotkey},
    {sect_clr, "menu-cursor",          255, &fl_clr.menu_cursor},
    {sect_clr, "menu-cursor-hotkey",   255, &fl_clr.menu_cursor_hotkey},
    {sect_clr, "menu-cursor-disabled", 255, &fl_clr.menu_cursor_disabled},
};

/* ------------------------------------------------------------ */

void GetProfileOptions (char *ini_name)
{
    char    *p, buf[16];
    int     i;

    if (infLoad (ini_name) < 0)
    {
        fly_error ("Error loading %s; terminating", ini_name);
    }

    // put defaults in place

    for (i=0; i<sizeof(boptions)/sizeof(boptions[0]); i++) *(boptions[i].value) = boptions[i].def;
    for (i=0; i<sizeof(ioptions)/sizeof(ioptions[0]); i++) *(ioptions[i].value) = ioptions[i].def;
    for (i=0; i<sizeof(soptions)/sizeof(soptions[0]); i++) *(soptions[i].value) = soptions[i].def;
    for (i=0; i<sizeof(xoptions)/sizeof(xoptions[0]); i++) if (xoptions[i].def != 255) *(xoptions[i].value) = xoptions[i].def;

    if (fl_opt.is_unix || fl_opt.platform == PLATFORM_OS2_X || fl_opt.platform == PLATFORM_OS2_X11) options.pseudographics = 2;

    // some defaults
    options.psw_file = str_sjoin (paths.user_libpath, "nftp.psw");
    options.history_file = str_sjoin (paths.user_libpath, "nftp.hst");
    //options.cc_log = str_sjoin (paths.user_libpath, "CC.txt");

    // retrieve options

    for (i=0; i<sizeof(boptions)/sizeof(boptions[0]); i++)
        infGetBoolean (boptions[i].sect, boptions[i].name, boptions[i].value);

    for (i=0; i<sizeof(ioptions)/sizeof(ioptions[0]); i++)
        infGetInteger (ioptions[i].sect, ioptions[i].name, ioptions[i].value);

    for (i=0; i<sizeof(soptions)/sizeof(soptions[0]); i++)
        infGetString (soptions[i].sect, soptions[i].name, soptions[i].value);

    for (i=0; i<sizeof(xoptions)/sizeof(xoptions[0]); i++)
        infGetHexbyte (xoptions[i].sect, xoptions[i].name, xoptions[i].value);


    if (infGetInteger (sect_psw, "encryption-type", &options.psw_enctype) < 0)
    {
        if (fl_opt.platform != PLATFORM_UNIX_TERM && fl_opt.platform != PLATFORM_UNIX_X11)
            options.psw_enctype = 2;
    }

    if (infGetBoolean (sect_options, "query-bfs-attributes-support", &options.query_bfsattrs) < 0)
    {
        if (fl_opt.platform == PLATFORM_BEOS_TERM) options.query_bfsattrs = TRUE;
        else                                       options.query_bfsattrs = FALSE;
    }

    if (options.user_agent == NULL)
    {
        strcpy (buf, NFTP_VERSION);
        str_strip2 (buf, " ");
        options.user_agent = str_join ("NFTP-", buf);
    }

    // post-config

    if (fl_opt.has_osmenu) fl_opt.menu_onscreen = FALSE;

    // INI file editor

    if (fl_opt.platform == PLATFORM_OS2_VIO)
    {
        options.texteditor = "tedit.exe";
        infGetString (sect_options, "text-editor-os2vio", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_OS2_PM)
    {
        options.texteditor = "e.exe";
        infGetString (sect_options, "text-editor-os2pm", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_OS2_X || fl_opt.platform == PLATFORM_OS2_X11)
    {
        options.texteditor = "xedit.exe";
        infGetString (sect_options, "text-editor-os2x", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_WIN32_CONS)
    {
        options.texteditor = "Notepad.exe";
        infGetString (sect_options, "text-editor-win32cons", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_WIN32_GUI)
    {
        options.texteditor = "Notepad.exe";
        infGetString (sect_options, "text-editor-win32gui", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_BEOS_TERM)
    {
        options.texteditor = "vi";
        infGetString (sect_options, "text-editor-beosterm", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_UNIX_TERM)
    {
        options.texteditor = "vi";
        infGetString (sect_options, "text-editor-unixterm", &options.texteditor);
    }

    if (fl_opt.platform == PLATFORM_UNIX_X11)
    {
        options.texteditor = "xedit";
        infGetString (sect_options, "text-editor-unix_x11", &options.texteditor);
    }

    // others

    if (options.log_trans)
    {
        if (infGetString (sect_options, "log-transfers-name", &p) == 0)
        {
            strcpy (options.log_trans_name, p);
            free (p);
        }
        else
        {
            strcpy (options.log_trans_name, paths.user_libpath);
            str_cats (options.log_trans_name, "nftp.fls");
        }
    }

    if (infGetString (sect_options, "bookmarks-file", &p) == 0)
    {
        strcpy (options.bmk_name, p);
        free (p);
    }
    else
    {
        strcpy (options.bmk_name, paths.user_libpath);
        str_cats (options.bmk_name, "nftp.bmk");
    }

    // registration information

    if ((cfg_get_string (CONFIG_NFTP, NULL, "registration-name"))[0] == '\0')
    {
        if (infGetString (sect_registration, "name", &p) == 0)
        {
            cfg_set_string (CONFIG_NFTP, NULL, "registration-name", p);
            free (p);
        }

        if (infGetString (sect_registration, "code", &p) == 0)
        {
            cfg_set_string (CONFIG_NFTP, NULL, "registration-code", p);
            free (p);
        }
    }

    // clear key definition table
    for (i=0; i<sizeof(options.keytable)/sizeof(int); i++)
    {
        options.keytable[i]  = KEY_NOTHING;
        options.keytable2[i] = KEY_NOTHING;
    }

    def_key_assignments ();
    key_assignments ();

    // retrieve colours
    if ((options.monochrome && !cmdline.colour) || cmdline.monochrome)
    {
        /*
        options.attr_pointer_marked_dir = VID_REVERSE;
        options.attr_pointer_marked     = VID_REVERSE;
        options.attr_pointer_dir        = VID_REVERSE;
        options.attr_pointer            = VID_REVERSE;
        options.attr_pointer_desc       = VID_REVERSE;
        options.attr_marked_dir         = VID_BRIGHT;
        options.attr_marked             = VID_BRIGHT;
        options.attr_dir                = VID_NORMAL;
        options.attr_description        = VID_NORMAL;
        options.attr_                   = VID_NORMAL;
        */
        options.attr_background         = VID_NORMAL;
        options.attr_status             = VID_NORMAL;
        options.attr_status2            = VID_REVERSE;
        options.attr_statmarked         = VID_REVERSE;
        options.attr_tr_info            = VID_NORMAL;
        options.attr_help               = VID_REVERSE;
        options.attr_status_local       = VID_NORMAL;

        options.attr_tp_file     = VID_NORMAL;
        options.attr_tp_dir      = VID_NORMAL;
        options.attr_tp_file_m   = VID_NORMAL;
        options.attr_tp_dir__m   = VID_NORMAL;
        options.attr_tp_file_p   = VID_REVERSE;
        options.attr_tp_dir__p   = VID_REVERSE;
        options.attr_tp_file_mp  = VID_REVERSE;
        options.attr_tp_dir__mp  = VID_REVERSE;

        options.attr_sp_file     = VID_NORMAL;
        options.attr_sp_dir      = VID_NORMAL;
        options.attr_sp_desc     = VID_NORMAL;
        options.attr_sp_file_m   = VID_NORMAL;
        options.attr_sp_dir__m   = VID_NORMAL;
        options.attr_sp_desc_m   = VID_NORMAL;
        options.attr_sp_file_p   = VID_REVERSE;
        options.attr_sp_dir__p   = VID_REVERSE;
        options.attr_sp_desc_p   = VID_REVERSE;
        options.attr_sp_file_mp  = VID_REVERSE;
        options.attr_sp_dir__mp  = VID_REVERSE;
        options.attr_sp_desc_mp  = VID_REVERSE;

        options.attr_cntr_header        = VID_REVERSE;
        options.attr_cntr_resp          = VID_NORMAL;
        options.attr_cntr_cmd           = VID_BRIGHT;
        options.attr_cntr_comment       = VID_BRIGHT;

        options.attr_bmrk_back          = VID_REVERSE;
        options.attr_bmrk_pointer       = VID_NORMAL;
        options.attr_bmrk_hostpath      = VID_REVERSE;
        options.attr_bmrk_hostpath_pointer = VID_BRIGHT;
    }

    infFree ();
}

/* ------------------------------------------------------------- */

static void def_key_assignments (void)
{
    /* -------------------------------------------------------------------
     Common keys
     --------------------------------------------------------------------- */

    options.keytable[_F1]  = KEY_HELP;
    options.keytable[_F2]  = KEY_GEN_HISTORY;
    options.keytable[_F3]  = KEY_FM_VIEW_INT;
    //options.keytable[_F4]  = KEY_GEN_SAVEMARK;
    options.keytable[_F5]  = KEY_COPY;
    options.keytable[_F6]  = KEY_MOVE;
    options.keytable[_F7]  = KEY_FM_MKDIR;
    options.keytable[_F8]  = KEY_FM_DELETE;
    options.keytable[_F9]  = KEY_MENU;
    options.keytable[_F10] = KEY_GEN_EXIT;

    options.keytable[_CtrlA] = KEY_FM_SELECTALL;
    options.keytable[_CtrlB] = KEY_GEN_BOOKMARKS;
    //tions.keytable[_CtrlC] SIGINT under Unix
    options.keytable[_CtrlD] = KEY_SHOW_OWNER;
    options.keytable[_CtrlE] = KEY_GEN_HISTORY;
    options.keytable[_CtrlF] = KEY_MENU;
    options.keytable[_CtrlG] = KEY_FM_MKDIR;
    //tions.keytable[_CtrlH] backspace
    //tions.keytable[_CtrlI] tab
    //options.keytable[_CtrlJ] identical to Enter under Unix
    options.keytable[_CtrlK] = KEY_GEN_LOGOFF;
    options.keytable[_CtrlL] = KEY_GEN_LOGIN;
    //tions.keytable[_CtrlM] enter
    options.keytable[_CtrlN] = KEY_GEN_GOTO;
    options.keytable[_CtrlO] = KEY_GEN_SHOW_DESC;
    options.keytable[_CtrlP] = KEY_GEN_MODE_SWITCH;
    options.keytable[_CtrlQ] = KEY_FM_COMPUTE_DIRSIZE;
    options.keytable[_CtrlR] = KEY_FM_REREAD;
    options.keytable[_CtrlS] = KEY_SELECT_CC;
    options.keytable[_CtrlT] = KEY_GEN_SHOWTIMESTAMPS;
    //options.keytable[_CtrlU] = KEY_FM_UPLOAD_EVERYTHING;
    options.keytable[_CtrlV] = KEY_FM_VIEW_INT;
    options.keytable[_CtrlW] = KEY_GEN_SPLITSCREEN;
    options.keytable[_CtrlX] = KEY_FM_DESELECTALL;
    //options.keytable[_CtrlY] =
    options.keytable[_CtrlZ] = KEY_SUSPEND;

    options.keytable['A'] = KEY_GEN_NOOP;
    options.keytable['B'] = KEY_GEN_SAVEMARK;
    options.keytable['C'] = KEY_FM_CHDIR;
    options.keytable['D'] = KEY_COPY;
    options.keytable['E'] = KEY_FM_SORT_REVERSE;
    options.keytable['F'] = KEY_FTPS_FIND;
    options.keytable['G'] = KEY_FM_DOWNLOAD_BYNAME;
    options.keytable['H'] = KEY_MAINHELP;
    options.keytable['I'] = KEY_GEN_INFORMATION;
    //options.keytable['J'] =
    options.keytable['K'] = KEY_SHORTHELP;
    options.keytable['L'] = KEY_GEN_OPEN_URL;
    options.keytable['M'] = KEY_FM_SELECT;
    options.keytable['N'] = KEY_FM_SORT_NAME;
    options.keytable['O'] = KEY_FM_LOAD_DESCRIPTIONS;
    options.keytable['P'] = KEY_FTPS_RECALL;
    options.keytable['Q'] = KEY_GEN_QUOTE;
    options.keytable['R'] = KEY_FM_RENAME;
    options.keytable['S'] = KEY_FM_SORT_SIZE;
    options.keytable['T'] = KEY_FM_SORT_TIME;
    //options.keytable['U'] = KEY_FM_UPLOAD;
    options.keytable['V'] = KEY_FM_VIEW_EXT;
    options.keytable['W'] = KEY_FM_DOWNLOAD_WGET;
    options.keytable['X'] = KEY_FM_SORT_EXT;
    options.keytable['Y'] = KEY_FM_SORT_UNSORT;
    options.keytable['Z'] = KEY_FM_CHLOCAL_DRIVE;

    options.keytable['?']    = KEY_HELP;
    options.keytable['>']    = KEY_PG_RIGHT;
    options.keytable['<']    = KEY_PG_LEFT;
    options.keytable['[']    = KEY_FM_GO_UP;
    options.keytable[']']    = KEY_FM_ENTER_DIR;
    options.keytable['\\']   = KEY_FM_GO_ROOT;
    options.keytable[_Space] = KEY_GEN_SWITCH_TO_CC;
    options.keytable[_Tab]   = KEY_GEN_SWITCH_LOCREM;

    options.keytable[_Insert]   = KEY_FM_SELECT;
    options.keytable[_Delete]   = KEY_FM_DELETE;
    options.keytable[_Plus]     = KEY_FM_MASK_SELECT;
    options.keytable[_Minus]    = KEY_FM_MASK_DESELECT;
    options.keytable[_Asterisk] = KEY_FM_INVERT_SEL;

    /* ---------------------------------------------------------------
     OS/2 and Win32 specific keys (obsolete)
     ----------------------------------------------------------------- */

    options.keytable2[_ShiftF1] = KEY_MAINHELP;
    //options.keytable2[_ShiftF5] = KEY_FM_UPLOAD;

    options.keytable2[_AltF1]   = KEY_FM_CHLOCAL_DRIVE;
    options.keytable2[_AltF2]   = KEY_GEN_HISTORY;
    options.keytable2[_AltF3]   = KEY_FM_VIEW_EXT;
    //options.keytable2[_AltF5]   = KEY_FM_DOWNLOAD_EVERYTHING;

    options.keytable2[_CtrlF1] = KEY_HELP;
    options.keytable2[_CtrlF3] = KEY_FM_SORT_NAME;
    options.keytable2[_CtrlF4] = KEY_FM_SORT_EXT;
    options.keytable2[_CtrlF5] = KEY_FM_SORT_TIME;
    options.keytable2[_CtrlF6] = KEY_FM_SORT_SIZE;
    options.keytable2[_CtrlF7] = KEY_FM_SORT_UNSORT;
    options.keytable2[_CtrlF8] = KEY_FM_SORT_REVERSE;

    //options.keytable2[_AltC]   = KEY_FM_CHDIR;
    //options.keytable2[_AltD]   = KEY_FM_DOWNLOAD_BYNAME;
    //options.keytable2[_AltF]   = KEY_GEN_USEFLAGS;
    //options.keytable2[_AltI]   = KEY_GEN_INFORMATION;
    //options.keytable2[_AltN]   = KEY_GEN_NOOP;
    //options.keytable2[_AltO]   = KEY_FM_LOAD_DESCRIPTIONS;
    //options.keytable2[_AltS]   = KEY_FM_SAVE_LISTING;
    //options.keytable2[_AltQ]   = KEY_GEN_QUOTE;

    options.keytable2[_CtrlRight]     = KEY_PG_RIGHT;
    options.keytable2[_CtrlLeft]      = KEY_PG_LEFT;
    options.keytable2[_CtrlPgDn]      = KEY_FM_ENTER_DIR;
    options.keytable2[_CtrlPgUp]      = KEY_FM_GO_UP;
    options.keytable2[_CtrlBackSlash] = KEY_FM_GO_ROOT;

    options.keytable2[_CtrlNumPlus]   = KEY_FM_SELECTALL;
    options.keytable2[_CtrlNumMinus]  = KEY_FM_DESELECTALL;

    options.keytable2[_AltRight]      = KEY_GEN_EXPAND_RIGHT;
    options.keytable2[_AltLeft]       = KEY_GEN_CONTRACT_LEFT;
    options.keytable2[_AltDown]       = KEY_GEN_EXPAND_DOWN;
    options.keytable2[_AltUp]         = KEY_GEN_CONTRACT_UP;

    /* -----------------------------------------------------------------
     Non-redefinable keys
     ------------------------------------------------------------------- */

    options.keytable[_Up]        = KEY_UP;
    options.keytable[_Down]      = KEY_DOWN;
    options.keytable[_Right]     = KEY_RIGHT;
    options.keytable[_Left]      = KEY_LEFT;
    options.keytable[_PgUp]      = KEY_PGUP;
    options.keytable[_PgDn]      = KEY_PGDN;
    options.keytable[_Home]      = KEY_HOME;
    options.keytable[_End]       = KEY_END;
    options.keytable[_Esc]       = KEY_ESC;
    options.keytable[_Enter]     = KEY_ENTER;
    options.keytable[_BackSpace] = KEY_BACKSPACE;

    /* -----------------------------------------------------------------
     Lynx-style definitions
     ------------------------------------------------------------------- */

    if (options.lynx_keys)
    {
        options.keytable[_Right] = KEY_FM_ENTER_DIR;
        options.keytable[_Left]  = KEY_FM_GO_UP;
    }
}

/* ------------------------------------------------------------- */

struct _kt {int v; char *n;};
static struct _kt kt[]=
{
    {KEY_NOTHING, "nothing"},

    {KEY_MENU, "enter-menu"},
    {KEY_HELP, "context-sensitive-help"},
    {KEY_MAINHELP, "main-help"},
    {KEY_SHORTHELP, "concise-help"},
    {KEY_INTERNAL_VERSION, "show-version"},
    {KEY_GEN_EDIT_INI, "edit-inifile"},
    {KEY_UPDATE_NFTP, "update-nftp"},

    {KEY_PG_RIGHT, "scroll-10-right"},
    {KEY_PG_LEFT, "scroll-10-left"},

    {KEY_GEN_SWITCH_TO_CC, "switch-to-cc"},
    {KEY_GEN_SWITCH_LOCREM, "switch-local-remote"},
    {KEY_GEN_MODE_SWITCH, "directory-mode-switch"},
    {KEY_PASSIVE_MODE, "passive-mode"},

    {KEY_GEN_BOOKMARKS, "bookmarks"},
    {KEY_GEN_LOGIN, "login"},
    {KEY_GEN_OPEN_URL, "open-url"},
    {KEY_GEN_NOOP, "no-operation"},
    {KEY_GEN_LOGOFF, "logoff"},
    {KEY_EDIT_PASSWORDS, "passwords"},
    {KEY_GEN_EXIT, "exit"},

    {KEY_COPY, "copy"},
    //{KEY_FM_DOWNLOAD, "download"},
    //{KEY_FM_UPLOAD, "upload"},
    //{KEY_FM_DOWNLOAD_EVERYTHING, "download-all-dirs"},
    {KEY_FM_DOWNLOAD_WGET, "download-wget"},
    //{KEY_FM_UPLOAD_EVERYTHING, "upload-all-dirs"},
    {KEY_FM_DOWNLOAD_BYNAME, "download-byname"},
    {KEY_EXPORT_MARKED, "export-marked"},

    {KEY_FM_SAVE_LISTING, "save-listing"},
    {KEY_GEN_INFORMATION, "information"},
    {KEY_GEN_HISTORY, "history"},
    {KEY_BINARY_ASCII, "switch-binary-ascii"},
    {KEY_GEN_USEFLAGS, "use-flags"},
    {KEY_GEN_SWITCH_PROXY, "use-proxy"},
    {KEY_GEN_QUOTE, "quote"},

    {KEY_GEN_SAVEMARK, "save-bookmark"},
    {KEY_GEN_SCREENREDRAW, "screen-redraw"},
    {KEY_GEN_SPLITSCREEN, "split-screen"},
    {KEY_GEN_SHOWTIMESTAMPS, "show-timestamps"},
    {KEY_GEN_SHOWMINISTATUS, "show-ministatus"},

    {KEY_FM_VIEW_INT, "view-int"},
    {KEY_FM_VIEW_EXT, "view-ext"},
    {KEY_FM_DELETE, "delete"},
    {KEY_FM_MKDIR, "mkdir"},
    {KEY_FM_RENAME, "rename"},
    {KEY_FM_SEARCH, "search"},
    {KEY_FM_REPEAT_SEARCH, "repeat-search"},
    {KEY_CHANGE_PERMISSIONS, "change-permissions"},

    {KEY_FM_SORT_NAME, "sort-name"},
    {KEY_FM_SORT_EXT, "sort-ext"},
    {KEY_FM_SORT_TIME, "sort-time"},
    {KEY_FM_SORT_SIZE, "sort-size"},
    {KEY_FM_SORT_UNSORT, "sort-unsort"},
    {KEY_FM_SORT_REVERSE, "sort-reverse"},

    {KEY_SYNC_BY_DOWNLOAD, "synchronize-local-with-remote"},
    {KEY_SYNC_BY_UPLOAD, "synchronize-remote-with-local"},

    {KEY_FM_REREAD, "reread"},
    {KEY_FM_ENTER_DIR, "enter-directory"},
    {KEY_FM_GO_ROOT, "go-root"},
    {KEY_FM_GO_UP, "go-up"},
    {KEY_FM_CHDIR, "change-directory"},
    {KEY_FM_CHLOCAL_DRIVE, "change-local-drive"},
    {KEY_FM_LOAD_DESCRIPTIONS, "load-descriptions"},
    {KEY_GEN_SHOW_DESC, "hide-descriptions"},
    {KEY_SHOW_OWNER, "show-owner"},

    {KEY_FM_SELECTALL, "select-all"},
    {KEY_FM_DESELECTALL, "deselect-all"},
    {KEY_FM_SELECT, "select"},
    {KEY_FM_MASK_SELECT, "select-by-filter"},
    {KEY_FM_MASK_DESELECT, "deselect-by-filter"},
    {KEY_FM_INVERT_SEL, "invert-selection"},

    {KEY_FTPS_FIND, "ftpsearch-find"},
    {KEY_FTPS_DEFINE_SERVERS, "ftpsearch-servers"},
    {KEY_FTPS_RECALL, "ftpsearch-recall"},
};

/* ------------------------------------------------------------- */

static void key_assignments (void)
{
    int    i, j, k, errcnt=0, nkt=sizeof(kt)/sizeof(struct _kt);
    char   sect_keys[]="keys", *p, *p1, *p2, *p3;

    for (j=0; j<nkt; j++)
    {
        if (infGetString (sect_keys, kt[j].n, &p) < 0) continue;
        p1 = p;
        for (i=1; i<=64; i++)
        {
            p2 = strchr (p1, ',');
            if (p2 != NULL) *p2 = '\0';
            p3 = strdup (p1);
            str_strip2 (p3, " ");
            if (p3[0] != '\0')
            {
                k = fly_keyvalue (p3);
                free (p3);
                if (k == -1)
                {
                    fly_ask_ok (0, "wrong key name: [%s]\n", p1);
                    errcnt++;
                }
                else
                {
                    options.keytable[k] = kt[j].v;
                }
            }
            if (p2 == NULL) break;
            p1 = p2 + 1;
        }
        free (p);
    }

    if (errcnt && fl_opt.has_console)
    {
        fprintf (stderr, "%d errors while processing key definitions\nPress ENTER...", errcnt);
        fgetc (stdin);
    }
}
