#include "types1.h"

#include "help.h"

#include <libintl.h>
#define M(string) gettext(string)

#define NFTP_VERSION     " 1.8 "

#define READ_RELAXED     1
#define READ_FORCED      2

#define PROGRESS_SCREEN  1
#define PROGRESS_MSG     2

#define LOCAL_WITH_REMOTE 1
#define REMOTE_WITH_LOCAL 2

#define FILE_FLAG_MARKED     0x0001
#define FILE_FLAG_DIR        0x0002
#define FILE_FLAG_LINK       0x0004
#define FILE_FLAG_REGULAR    0x0008
#define FILE_FLAG_COMPSIZE   0x0010
#define FILE_FLAG_MDTM       0x0020

#define SYS_UNKNOWN        0
#define SYS_UNIX           1
#define SYS_IBMOS2FTPD     2
#define SYS_WINNT          3
#define SYS_WFTPD          4
#define SYS_OS2NEOLOGIC    5
#define SYS_POWERWEB       6
#define SYS_WINNTDOS       7
#define SYS_WORLDGROUP     8
#define SYS_UNISYS_A       9
#define SYS_MACOS_PETER   10
#define SYS_AS400         11
#define SYS_VMS_MULTINET  12
#define SYS_VMS_UCX       13
#define SYS_VMS_MADGOAT   14
#define SYS_MPEIX         15
#define SYS_SQUID         16
#define SYS_NSPROXY       17
#define SYS_BORDERMANAGER 18
#define SYS_MLST          19
#define SYS_VMSP          20
#define SYS_NETWARE       21
#define SYS_MVS           22
#define SYS_MVS_NATIVE    23
#define SYS_MVS_NATIVE1   24
#define SYS_MVS_NATIVE2   25
#define SYS_INKTOMI       26
#define SYS_APACHE        27
#define SYS_ISA2000       28

#define MODE_RAW         1
#define MODE_PARSED      2
#define MODE_DESC        3

#define BROWSER_EXTERNAL    1
#define BROWSER_INTERNAL    2

#define VIEW_REMOTE          0
#define VIEW_CONTROL         1
#define VIEW_LOCAL           2

#ifndef MAX_PATH
#define MAX_PATH        260
#endif

#define CONFIG_NFTP     0

//#define T_BINARY        1
//#define T_ASCII         2

#define SORT_UNSORTED   0
#define SORT_NAME       1
#define SORT_EXT        2
#define SORT_SIZE       3
#define SORT_TIME       4

#define DOWN             1
#define UP               2
#define LIST             3
#define LSATTRS          4
#define SETATTRS         5
#define RETATTRS         6

/* ------------------------------------------------------------------- */

#define ERR_OK            0 /* no errors detected */
#define ERR_TRANSIENT    -1 /* operation failed. may be timeout or */
                            /* something similar. makes sense to retry */
#define ERR_FATAL        -2 /* fatal error. stop attempting to connect */
#define ERR_SKIP         -3 /* skipped by user, do not retry and proceed */
#define ERR_CANCELLED    -4 /* cancelled by user, do not retry and stop */
#define ERR_AUTHENTICATE -5 /* needs authentication */
#define ERR_RESTFAIL     -6 /* restart has failed */
#define ERR_FIXPORT      -7 /* PORT command might contain wrong address after reconnect */

/* -------------------------------------------------------------

#define NLINES        (video_vsize() - options.bottom_status_style \
    - options.top_status_style - (fl_opt.has_osmenu ? 0 : fl_opt.menu_onscreen))
#define SLINE         (options.top_status_style \
    + (fl_opt.has_osmenu ? 0 : fl_opt.menu_onscreen))
    */

#define WINTITLE_PREFIX (fl_opt.is_unix ? "nftp: " : "")

#define is_anonymous    (strcmp (site.u.userid, options.anon_name) == 0 || \
                         strcmp (site.u.userid, "ftp") == 0)

#define is_remote_unix  (site.system_type == SYS_UNIX)

#define is_subdir(d1,d2) (strcmp(d1.name, d2.name, strlen(d1.name) == 0)

#define is_batchmode (cmdline.batchmode || site.batch_mode)

#define markable(f) \
    ( ((f).flags & (FILE_FLAG_DIR | FILE_FLAG_REGULAR)) && \
      strcmp ((f).name, "..") != 0 )

#define is_time1980 (fl_opt.platform == PLATFORM_OS2_VIO || fl_opt.platform == PLATFORM_OS2_PM || \
    fl_opt.platform == PLATFORM_OS2_X || fl_opt.platform == PLATFORM_OS2_X11 || \
    fl_opt.platform == PLATFORM_WIN32_CONS || fl_opt.platform == PLATFORM_WIN32_GUI)

#define is_OS2 (fl_opt.platform == PLATFORM_OS2_VIO || fl_opt.platform == PLATFORM_OS2_PM || \
    fl_opt.platform == PLATFORM_OS2_X || fl_opt.platform == PLATFORM_OS2_X11)

#define T_01_01_1980 315532800

/* ------------------------------------------------------------------------ */

extern struct _display        display;
extern struct _options        options;
extern struct _status         status;
extern struct _cmdline        cmdline;
extern struct _sort           sort;
extern struct _paths          paths;
extern FILE                   *dbfile;
extern struct _item           *main_menu;

/* -------------------------------------------------------------------- */

int  main (int argc, char *argv[], char **envp);
void do_key (int key);
RC transfer (int where, trans_item *T, int nt, int noisy);
RC get_file (char *r_filename, char *r_dirname,
             char *l_filename, char *l_dirname,
             int64_t size, struct tm timestamp);
RC put_file (char *r_filename, char *r_dirname,
             char *l_filename, char *l_dirname,
             int64_t size, struct tm timestamp);
void analyze_listing (char *buf, char *dirname, int type);
void update (int flush);
void update_switches (void);
int p_nl (void);
int p_nlf (void);
int p_sl (void);
int p_slf (void);

int  view_file (int browser, char *name);

int  init (int argc, char *argv[]);
void terminate (void);
void determine_paths (char *argv0);
void usage (void);
void check_args (int argc, char *argv[]);
void def_colors (void);
void clear_tmp (void);

int  ask_logoff (void);
void put_message (char *msg, ...);
void log_transfers (char *format, ...);
RC   change_dir (char *dirname);
RC   retrieve_dir (char *dirname);
RC   get_dirname (char *dirname);
RC   attach_descriptions (int nf);
void ScreenUpdate (int code);
void DisplayConnectionHistory (void);
void GetProfileOptions (char *ini_name);
void PutLineIntoResp1 (char *format, ...);
void PutLineIntoResp2 (char *format, ...);
void PutLineIntoResp3 (char *format, ...);
void init_transfer_stats (void);
void set_local_path (char *path);
void help (int nh);
void sort_remote_files (char *bar_position);
void sort_local_files (void);
void build_local_filelist (char *prevdir);
void quicksearch (directory *d);
RC   cache_file (int nd, int nf);
RC   set_wd (char *newdir, int noisy);
void try_set_local_cursor (char *name);
void try_set_remote_cursor (char *name);
void gather_marked (int sub_only, int *marked_no, int *marked_dirs, int64_t *marked_size);
void fix_screen_pos (void);
int  do_get (char *url);
int  do_put (char *url);
int  do_delete (char *url);
void display_bottom_status_line (void);
void display_top_status_line (void);
void set_view_mode (int mode);
void adjust_menu_status (void);
void init_config (void);
void write_config (void);
void load_menu (void);
void initialize (int *argc, char **argv[], char ***envp);
void config_fly (void);
void set_dir_size (int d, int f);
int  runscript (char *filename);
int  find_directory (char *s);
int delete_subdir (char *path, int *noisy);
int create_dir (char *d);
/* delete everything under this path (including this path) */
int delete_remote_subdir (char *dirname, char *filename,
                          int *noisy_notempty, int *noisy_readonly);
RC traverse_directory (char *dir, int mark);
RC r_chdir (char *dirname, int mark);
void ask_refresh (char *fn);
/* -------------------------------------------------------------
 -- cache_dir: makes sure that specified directory (full path)
 is cached. returns 0 on success, negative values on errors */
RC cache_dir (char *dirname);

/* -------------------------------------------------------------
 walks into subdirs traversing entire tree */
int cache_subtree (int dirno, int fileno);

void bookmark_add (url_t *u);
int  bookmark_select (url_t *u);
int  bookmark_nickname (char *nickname, url_t *u);

void update_local_list (void);
void update_remote_list (void);

/* this function removes directories those names start with 'path'
 from the cache. it always returns 0 */
int invalidate_directory (char *path, int subdirs);
