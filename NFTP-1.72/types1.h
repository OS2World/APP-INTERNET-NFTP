#ifndef TYPES1_H_INCLUDED
#define TYPES1_H_INCLUDED

#include <time.h>
#include <asvtools.h>

typedef struct
{
    int     no;        /* original no.; used to restore original order after sorts */
    char    *name;     /* file name, null-terminated */
    int     extension; /* extension, displacement in [name] */
    int     rights;    /* Unix access permissions */
    int64_t size;      /* file size, in bytes */
    time_t  mtime;     /* modification time, UTC */
    char    *rawtext;  /* raw text representation */
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
   int  current_file, first_item;
}
directory;

struct _local_cache
{
    int             ld, lda;
    directory       *L;
};

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
};

struct _site
{
    /* live parameters */
    int             connected;
    int             set_up;
    int             cc_sock;
    //int             transfer_mode;           // BINARY/ASCII
    int             batch_mode;
    int             fallback;

    /* site data */
    url_t            u;
    int              system_type;
    int              date_fmt;
    unsigned long    l_ip;
    
    struct _features features;

    /* files/directories */
    directory       *dir;
    int             cdir; /* current directory index */
    int             ndir; /* no. of directories */
    int             maxndir; /* max no. of directories */
    int             sortmode;
    int             sortdirection;
    double          speed;
    void            *chunks;
};

struct _cchistory
{
    char *        *ptr;
    time_t        *tms;
    int           n, na;
    int           fline, shift;
};

struct _local
{
    directory       dir;
    int             sortmode;
    int             sortdirection;
    int             drive;
};

#define TF_OVERWRITE   0x00010000
#define TF_HIDDEN      0x00020000
#define TF_DIRECTORY   0x00040000

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

struct _display
{
   int           tabsize;
   int           rshift, lshift;
   int           dir_mode, view_mode;
   char          quicksearch[80];
};

struct _status
{
    int   use_flags, use_proxy, load_descriptions, split_view;
    int   show_times, show_ministatus, passive_mode, traffic_shaping;
    int   resolve_symlinks, sync_delete, sync_subdirs, binary;

    int   transfer_paused, usage_interval;
};

struct _firewall
{
    char            userid[128];
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
    int   menu_onscreen, hide_dates_too, full_frames;
    int   as400_put, ask_walktree, preserve_permissions_download;
    int   preserve_permissions_upload, vms_remove_version;
    int   flush_http_cache, try_ftpext, ask_exit, relaxed_mirror;
    int   query_bfsattrs, use_bfs_files, ask_reread, ask_logoff;
    int   os390_hide, guess_cyrillic, delayed_passwords;
    int   show_hw_cursor, backups, shorten_names, new_transfer, use_MDTM;

    int   defaultsort, default_localsort;
    int   defaultdirmode;
    int   retry_pause, data_timeout, cc_timeout;
    int   desc_size_limit, desc_time_limit, desc_indent;
    int   bottom_status_style, top_status_style;
    int   as400_date_fmt, stalled_interval;
    int   defaultport, batch_limit, backups_limit;
    int   delfile_limit, deldir_limit;

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
    
    char  attr_pointer_marked_dir;
    char  attr_pointer_marked;
    char  attr_pointer_dir;
    char  attr_pointer;
    char  attr_pointer_desc;
    
    char  attr_marked_dir;
    char  attr_marked;
    char  attr_dir;
    char  attr_;
    char  attr_description;

    char  attr_background;
    char  attr_status;
    char  attr_status2;
    char  attr_status_local;
    char  attr_statmarked;
    char  attr_tr_info;
    char  attr_help;
    
    char  attr_tp_file;
    char  attr_tp_dir;
    char  attr_tp_file_m;
    char  attr_tp_dir__m;
    char  attr_tp_file_p;
    char  attr_tp_dir__p;
    char  attr_tp_file_mp;
    char  attr_tp_dir__mp;

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
    int64_t  tsize;
    int64_t  reget;
    int64_t  trans;
    double   start;
}
stats;

struct _paths
{
    // Under Unix, /usr/lib/nftp or /usr/local/lib/nftp or ~/.nftp
    // Under OS/2, /apps/nftp
    // Under BeOS, /boot/beos/etc or /boot/home/config/etc
    char *system_libpath;
    // Under Unix/BeOS, ~/.nftp.
    // Under OS/2, /apps/nftp
    char *user_libpath;
};

typedef int RC;

#endif
