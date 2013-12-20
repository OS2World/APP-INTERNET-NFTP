#ifndef TYPES1_H_INCLUDED
#define TYPES1_H_INCLUDED

#include <time.h>
#include <asvtools.h>

#define MAX_SITE    8

typedef struct
{
    int     no;        /* original no.; used to restore original order after sorts */
    char    *name;     /* file name, null-terminated */
    int     extension; /* extension, displacement in [name] */
    int     rights;    /* Unix access permissions */
    char    *owner;    /* Unix file owner */
    char    *group;    /* Unix file group */
    int64_t size;      /* file size, in bytes */
    time_t  mtime;     /* modification time, local(!) */
    char    *rawtext;  /* raw text representation; NULL for local files */
    int     flags;     /* various flags */
    char    *cached;   /* if viewed previously */
    char    *desc;     /* description */
    int     lrecl;     /* LRECL for OS/390 datasets */
}
file;

typedef struct
{
   char *name;
   int  nfiles;
   file *files;
   int  current;
   int  first;
}
directory;

struct _site
{
    /* live parameters */
    int             set_up;
    int             connected;
    time_t          updated;

    /* site data */
    url_t            u;
    int              system_type;
    int              date_fmt; /* used for AS/400 servers only */

    int              cc_sock;
    unsigned long    l_ip;

    struct _features
    {
        int rest_works;
        int appe_works;
        int chmod_works;
        int utime_works;
        int chall_works;
        int has_feat;
        int has_mdtm;
        int has_size;
        int has_rest;
        int has_tvfs;
        int has_mlst;
        int unixpaths;
        int has_bfs1;
    }
    features;

    struct
    {
        char *        *ptr;
        time_t        *tms;
        int           n, na;
    }
    CC;

    /* files/directories */
    directory *dir; /* directory cache */
    int       ndir; /* no. of directories */
    int       cdir; /* current directory */
    int       maxndir; /* max no. of directories */
    double    speed;
    int       sortmode;
    int       sortdirection;
};

struct _local
{
    directory       dir;
    int             sortmode;
    int             sortdirection;
    //int             drive;
};

struct _local_cache
{
    int             ld, lda;
    directory       *L;
};

#define TF_OVERWRITE   0x00010000

typedef struct
{
    char    *r_dir, *r_name; // both are fully qualified, malloc()ed
    char    *l_dir, *l_name; // both are fully qualified, malloc()ed
    time_t  mtime; // standard time, as returned by time()
    int64_t size; // in  bytes
    int     perm; // Unix permissions
    char    *description; // in malloc()ed space
    file    *f;
    int     flags;
}
trans_item;

struct _sort
{
    int mode, direction;
};

/* how display is managed: there are two panels. They may be either
 displayed simultaneously (status.split_view=TRUE), or not. In second
 case user has to press <tab> to switch between them (just like in
 split mode). Every panel can display one of the following things:
 -2         - nothing
 -1         - current local directory
 0..MAX_SITE-1 - remote server view
 Separate control connection view is gone; it will be displayed
 in a window

 Every panel has associated local view. It may display either this
 local view or any of the server views.
 */
enum {V_LEFT=0, V_RIGHT=1}; /* these serve for display.view and local ! */
struct _display
{
   int    tabsize;
   int    rshift, lshift;
   char   quicksearch[80];
   int    view[2]; /* state for left (V_LEFT) and right (V_RIGHT) panels */
   int    cursor; /* 0 - left, 1 - right */
   int    parsed; /* whether to display nonsplit panels in parsed view */
};

#define is_empty  (display.view[display.cursor] == -2)
#define is_local  (display.view[display.cursor] == -1)
#define is_remote (display.view[display.cursor] >= 0)

struct _status
{
    int   resolve_symlinks[MAX_SITE], use_flags[MAX_SITE];
    int   use_proxy[MAX_SITE], passive_mode[MAX_SITE];
    int   load_descriptions, split_view, binary;
    int   show_times, show_ministatus, show_owner;
    //int   traffic_shaping;
    int   sync_delete, sync_subdirs;

    int   transfer_paused, usage_interval;
    int   batch_mode;
};

struct _firewall
{
    char            userid[32];
    char            password[32];
    unsigned long   fwip;
};

struct _options
{
    // various flags
    int   debug, login_bell, transfer_pause, log_trans;
    int   save_marks, autocontrol, show_serverinfo;
    int   preserve_timestamp_download, preserve_timestamp_upload;
    int   dns_lookup, lowercase_names, use_termcap1;
    int   start_prompt, english_menu, menu_open_sub, pseudographics;
    int   fwbug1, passworded_wget, lynx_keys, monochrome, mouse;
    int   autopaste, twopanels, slowlink, keep_winpos, keep_winsize;
    int   hotmouse_menu, show_cc_time, show_dotfiles;
    int   hide_dates_too, full_frames, show_servername, show_panel_headings;
    int   as400_put, ask_walktree, preserve_permissions_download;
    int   preserve_permissions_upload, vms_remove_version, create_enters;
    int   flush_http_cache, try_ftpext, ask_exit, relaxed_mirror;
    int   query_bfsattrs, use_bfs_files, ask_reread, ask_logoff;
    int   os390_hide, guess_cyrillic, log_cc, fxp_skips, auto_refresh;

    int   defaultsort, default_localsort;
    int   defaultdirmode;
    int   retry_pause, data_timeout, cc_timeout;
    int   desc_size_limit, desc_time_limit, desc_indent;
    int   as400_date_fmt, stalled_interval;
    int   defaultport, batch_limit;

    char  *version;

    char  *anon_name, *anon_pass;

    int   firewall_type;
    char  *fire_server;
    char  *fire_login;
    char  *fire_passwd;
    int   fire_port/*, PASV_always*/;

    char  log_trans_name[2048];
    char  *download_path;
    char  *texteditor;
    char  *descfile;
    char  *wget_command;
    char  *transfer_complete_action;
    char  *user_agent;
    char  *selection_mask;

    char  bmk_name[2048];

    int   history_active;
    int   history_entries_watermark;
    int   history_sites_watermark;
    int   history_persite_watermark;
    int   history_everything;
    char  *history_file;

    int   psw_enctype; // 0 - password keeping is disabled
    char  *psw_file;

    int   ftps_show_servers;

    char  attr_background;
    char  attr_status;
    char  attr_status2;
    char  attr_status_local;
    char  attr_statmarked;
    char  attr_tr_info;
    char  attr_help;

    char  attr_tp_header;
    char  attr_tp_file;
    char  attr_tp_dir;
    char  attr_tp_file_m;
    char  attr_tp_dir__m;
    char  attr_tp_file_p;
    char  attr_tp_dir__p;
    char  attr_tp_file_mp;
    char  attr_tp_dir__mp;

    char  attr_sp_header;
    char  attr_sp_file;
    char  attr_sp_dir;
    char  attr_sp_desc;
    char  attr_sp_file_m;
    char  attr_sp_dir__m;
    char  attr_sp_desc_m;
    char  attr_sp_file_p;
    char  attr_sp_dir__p;
    char  attr_sp_desc_p;
    char  attr_sp_file_mp;
    char  attr_sp_dir__mp;
    char  attr_sp_desc_mp;

    char  attr_cntr_header;
    char  attr_cntr_resp;
    char  attr_cntr_cmd;
    char  attr_cntr_comment;

    char  attr_bmrk_back;
    char  attr_bmrk_pointer;
    char  attr_bmrk_hostpath;
    char  attr_bmrk_hostpath_pointer;

    int   keytable [512];
    int   keytable2 [512];
};

struct _cmdline
{
    int   monochrome;
    int   colour;
    int   slowlink;
    int   english;
    char  *language;
    int   batchmode;
};

typedef struct
{
    int64_t tsize;
    int64_t reget;
    int64_t trans;
    double  start;
}
stats;

struct _paths
{
    // Under Unix, /usr/lib/nftp2 or /usr/local/lib/nftp2 or ~/.nftp2
    // Under OS/2, /apps/nftp
    // Under BeOS, /boot/beos/etc/nftp2 or /boot/home/config/etc/nftp2
    char *system_libpath;
    // Under Unix/BeOS, ~/.nftp2.
    // Under OS/2, /apps/nftp
    char *user_libpath;
};

typedef int RC;

#endif
