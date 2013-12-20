#include <fly/fly.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>

#include "nftp.h"
#include "licensing.h"
#include "local.h"
#include "net.h"
#include "history.h"
#include "keys.h"
#include "auxprogs.h"
#include "network.h"
#include "passwords.h"

#ifdef __BEOS__
        #define USR_PATH           "/boot/beos/etc"
        #define USR_LOCAL_PATH     "/boot/home/config/etc"
#else
        #define USR_PATH           "/usr/lib"
        #define USR_LOCAL_PATH     "/usr/local/lib"
#endif

#ifdef __MINGW32__
#include "getopt.h"
#endif

void update_inifile (char *inifile, char *ifile);

#define ACTION_DOWNLOAD        1
#define ACTION_TESTONLY        2
#define ACTION_UPLOAD          3
#define ACTION_CMDLIST         5
#define ACTION_NICK_BOOK       6
#define ACTION_NICK_HIST       7
#define ACTION_LOGIN           8
#define ACTION_OPEN_BOOKMARKS  9
#define ACTION_OPEN_HISTORY    10
#define ACTION_BADAGS          11

static int     action, disc_after;
static char    *optarg1, *wintitle;

static struct
{
    char *name;
    int  def;
    int  key;
    int  *value;
}
runtime_flags[] =
{
    {"use-flags",         TRUE,  KEY_GEN_USEFLAGS,       &status.use_flags},
    {"use-proxy",         FALSE, KEY_GEN_SWITCH_PROXY,   &status.use_proxy},
    {"show-descriptions", TRUE,  KEY_GEN_SHOW_DESC,      &status.load_descriptions},
    {"split",             TRUE,  KEY_GEN_SPLITSCREEN,    &status.split_view},
    {"show-timestamps",   TRUE,  KEY_GEN_SHOWTIMESTAMPS, &status.show_times},
    {"show-ministatus",   TRUE,  KEY_GEN_SHOWMINISTATUS, &status.show_ministatus},
    {"passive-mode",      FALSE, KEY_PASSIVE_MODE,       &status.passive_mode},
    {"traffic-limiting",  FALSE, KEY_TRAFFIC_LIMIT,      &status.traffic_shaping},
    {"resolve-symlinks",  FALSE, KEY_RESOLVE_SYMLINKS,   &status.resolve_symlinks},
    {"sync-delete",       TRUE,  KEY_SYNC_DELETE,        &status.sync_delete},
    {"sync-subdirs",      TRUE,  KEY_SYNC_SUBDIRS,       &status.sync_subdirs},
    {"binary",            TRUE,  KEY_GEN_CHANGE_TRANSFER_MODE, &status.binary},
    {NULL,                FALSE, KEY_PAUSE_TRANSFER,     &status.transfer_paused},
};

/* ------------------------------------------------------------- */

void initialize (int *argc, char **argv[], char ***envp)
{
    char    buf[512];
    char    inifile[MAX_PATH], ifile[MAX_PATH];
#ifdef __MINGW32__    
    WSADATA wsa;
#endif

    //putenv ("TZ=GMT0");
    //tzset ();
    
    // first we need to initialize FLY variables
    fly_initialize ();
    fl_opt.appname = "nftp";
    
    if (fl_opt.has_console)
        fprintf (stderr, "\nNFTP - Version%sof %s, %s"
                 "\nCopyright (C) Sergey Ayukov 1994-2003.\n\n",
                 NFTP_VERSION, __DATE__, __TIME__);

    fly_process_args (argc, argv, envp);
    
    // checking command-line arguments
    check_args (*argc, *argv);
    
    // find where our config files are
    determine_paths ((*argv)[0]);

    // delete old temporary files
    clear_tmp ();
    
    // loading configuration files
    init_config ();

    // initilialize debug subsystem if specified
    if (options.debug)
    {
        snprintf1 (buf, sizeof(buf), "%s/debug", paths.user_libpath);
#ifdef __WIN32__        
        mkdir (buf);
#else
        mkdir (buf, 0700);
#endif
        snprintf1 (buf, sizeof(buf), "%s/debug/nftp.dbg.%u", paths.user_libpath, (int)getpid ());
        dbfile = fopen (buf, "w");
        tools_debug = dbfile;
    }

    // load language-specific things
    nls_init (paths.system_libpath, paths.user_libpath);
    load_menu ();

    // check for correct NFTP.INI version
    strcpy (buf, NFTP_VERSION);
    str_strip2 (buf, " ");
    //if (str_numchars (buf, '.') > 1)  *(strrchr (buf, '.')) = '\0';
    if (strcmp (options.version, buf) != 0)
    {
        // looking for nftp.i file in user dir, then in system dir
        strcpy (ifile, paths.user_libpath);
        str_cats (ifile, "nftp.i");
        if (access (ifile, R_OK) != 0)
        {
            strcpy (ifile, paths.system_libpath);
            str_cats (ifile, "nftp.i");
            if (access (ifile, R_OK) != 0)
                fly_error (MSG(M_INI_CANT_FIND_NFTP_I),
                        paths.user_libpath, paths.system_libpath);
        }
        strcpy (inifile, paths.user_libpath);
        str_cats (inifile, "nftp.ini");
        // ask user and update ini file
        //if (fl_opt.has_console) // otherwise it's too annoying
        //{
        //    fprintf (stderr, MSG(M_INI_CONFIG_VERSION), inifile, options.version, buf);
        //    fprintf (stderr, MSG(M_INI_UPDATE), inifile, ifile);
        //    fgets (buf, sizeof(buf), stdin);
        //    switch (buf[0])
        //    {
        //    case 'Q': case 'q': exit (0);
        //    case 'N': case 'n': break;
        //    case 'Y': case 'y': default:
        //        update_inifile (inifile, ifile);
        //    }
        //}
        //else
        //{
            update_inifile (inifile, ifile);
        //}
        GetProfileOptions (inifile);
    }
    
    config_fly ();

#ifdef __MINGW32__    
    WSAStartup (MAKEWORD(1,1), &wsa);
#endif
}

/* ------------------------------------------------------------- */

void check_args (int argc, char *argv[])
{
    int c;
    
    // initialize values
    cmdline.monochrome     = FALSE;
    cmdline.colour         = FALSE;
    cmdline.slowlink       = FALSE;
    cmdline.english        = FALSE;
    cmdline.language       = NULL;
    cmdline.batchmode      = FALSE;
    
    // analyze arguments
    action  = 0;
    disc_after = 0;
    
    optind  = 1;
    opterr  = 0;
    while ((c = getopt (argc, argv, "f:F:g:G:p:P:@:h:b:l:mctesB")) != -1)
    {
        switch (c)
        {
        case 'f':
        case 'g':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_DOWNLOAD;
                optarg1 = strdup (optarg);
            }
            break;
            
        case 'F':
        case 'G':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_DOWNLOAD;
                disc_after = 1;
                optarg1 = strdup (optarg);
            }
            break;
            
        case 'p':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_UPLOAD;
                optarg1 = strdup (optarg);
            }
            break;
            
        case 'P':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_UPLOAD;
                disc_after = 1;
                optarg1 = strdup (optarg);
            }
            break;
            
        case '@':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_CMDLIST;
                optarg1 = strdup (optarg);
            }
            break;
            
        case 'b':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_NICK_BOOK;
                optarg1 = strdup (optarg);
            }
            break;

        case 'h':
            if (action)
            {
                action = ACTION_BADAGS;
            }
            else
            {
                action = ACTION_NICK_HIST;
                optarg1 = strdup (optarg);
            }
            break;

        case 'l':
            cmdline.language = strdup (optarg);
            break;

        case 't':
            action = ACTION_TESTONLY;
            break;
            
        case 'm':
            cmdline.monochrome = TRUE;
            break;
            
        case 'c':
            cmdline.colour = TRUE;
            break;

        case 's':
            cmdline.slowlink = TRUE;
            break;

        case 'e':
            cmdline.english = TRUE;
            break;

        case 'B':
            cmdline.batchmode = TRUE;
            break;

        default:
        case ':':
        case '?':
            action = ACTION_BADAGS;
        }
    }

    // check if there's an argument (i.e. site to login)
    if (argv[optind] != NULL)
    {
        if (action)
            action = ACTION_BADAGS;
        else
            action = ACTION_LOGIN;
        optarg1 = strdup (argv[optind]);
    }
}

/* ------------------------------------------------------------- */

int init (int argc, char *argv[])
{
    char    *p;
    int     rc = 0, oldmode;
    url_t   u;
    int     x0=-1, y0=-1, r=-1, c=-1;
    struct hostent     *remote;
    char    buffer[MAX_PATH];

    if (action == ACTION_TESTONLY) exit (0);
    if (action == ACTION_BADAGS)
    {
        usage ();
        exit (1);
    }

    if (options.keep_winsize)
    {
        r = cfg_get_integer (CONFIG_NFTP, fl_opt.platform_nick, "rows");
        c = cfg_get_integer (CONFIG_NFTP, fl_opt.platform_nick, "cols");
        if (r == 0 && c == 0) r = -1, c = -1;
    }

    if (options.keep_winpos)
    {
        x0 = cfg_get_integer (CONFIG_NFTP, fl_opt.platform_nick, "x0");
        y0 = cfg_get_integer (CONFIG_NFTP, fl_opt.platform_nick, "y0");
        if (x0 == 0 || y0 == 0) x0 = -1, y0 = -1;
    }

    p = cfg_get_string (CONFIG_NFTP, fl_opt.platform_nick, "font");
    if (p[0] == '\0') p = NULL;

    fly_init (x0, y0, r, c, p);
    if (options.show_hw_cursor) video_cursor_state (1);
    fly_mouse (options.mouse);
    wintitle = get_window_name ();

    if (fl_opt.platform == PLATFORM_OS2_VIO)
    {
        strcpy (buffer, paths.system_libpath);
        str_cats (buffer, "nftp.ico");
        if (access (buffer, R_OK) == 0)
            set_icon (buffer);
    }

    if (main_menu != NULL)
    {
        menu_activate (main_menu);
        adjust_menu_status ();
    }
    
    display.dir_mode = options.defaultdirmode;
    display.view_mode = VIEW_CONTROL;
    
    display.rshift = 0;
    display.lshift = 0;
    display.tabsize = 8;
    if (options.slowlink)
        set_view_mode (VIEW_REMOTE);
    else
        set_view_mode (VIEW_CONTROL);
    site.maxndir = 1024;
    site.dir = malloc (sizeof(directory)*site.maxndir);

    // ignore "broken PIPE" signals
    signal (SIGPIPE, SIG_IGN);

    set_window_name ("NFTP%s(C) Copyright Sergey Ayukov", NFTP_VERSION);

    local.dir.files = NULL;
    local.sortmode = abs (options.default_localsort);
    if (options.default_localsort >= 0)
        local.sortdirection = 1;
    else
        local.sortdirection = -1;
    build_local_filelist (NULL);

    site.batch_mode = FALSE;
    site.chunks = NULL;

    PutLineIntoResp2 ("NFTP Version%s(%s, %s) -- %s", NFTP_VERSION, __DATE__, __TIME__, fl_opt.platform_name);
    PutLineIntoResp1 ("Copyright (C) 1994-2003 Sergey Ayukov <asv@ayukov.com>");
    PutLineIntoResp1 ("Portions Copyright (C) Eric Young <eay@cryptsoft.com>");
    //PutLineIntoResp1 ("Portions Copyright (C) Martin Nicolay <martin@osm-gmbh.de>");
    status.usage_interval = 0;
    if (License.status == LIC_NO || License.status == LIC_DELAYED)
        status.usage_interval = history_usage();
    if (License.status == LIC_VERIFIED || License.status == LIC_DELAYED)
    {
        PutLineIntoResp1 (MSG(M_REGISTERED), License.name == NULL ? "" : License.name);
    }
    else
    {
        if (status.usage_interval >= SHAREWARE_TRIAL-5)
            PutLineIntoResp3 (MSG(M_UNREGISTERED2), status.usage_interval);
        else
            PutLineIntoResp1 (MSG(M_UNREGISTERED2), status.usage_interval);
    }
    if (!fl_opt.has_osmenu)
        PutLineIntoResp1 (MSG(M_RESP_F9_FOR_MENU));
    update (1);

    if (options.firewall_type != 0)
    {
        if (options.fire_server[0] == '\0')
        {
            fly_ask_ok (ASK_WARN, MSG(M_PROXY_ISNT_SPECIFIED));
            options.firewall_type = 0;
        }
        else
        {
            if (strspn (options.fire_server, " .0123456789") == strlen (options.fire_server))
            {
                firewall.fwip = inet_addr (options.fire_server);
            }
            else
            {
                PutLineIntoResp2 (MSG(M_RESP_LOOKING_UP), options.fire_server);
                remote = gethostbyname (options.fire_server);
                if (remote == NULL)
                {
                    PutLineIntoResp2 (MSG(M_RESP_CANNOT_RESOLVE), options.fire_server);
                    options.firewall_type = 0;
                }
                else
                {
                    firewall.fwip = *((unsigned long *)(remote->h_addr));
                    PutLineIntoResp2 (MSG(M_RESP_FOUND), remote->h_name);
                }
            }
        }
    }

    // read password cache
    psw_read ();
        
    // analyze arguments
    switch (action)
    {
    case 0:
        if (options.download_path != NULL)
        {
            set_local_path (options.download_path);
        }
        else
        {
            p = cfg_get_string (CONFIG_NFTP, "", "local-directory");
            if (p[0] != '\0')
                set_local_path (p);
        }
        build_local_filelist (NULL);
        switch (options.start_prompt)
        {
        case 1:  return FMSG_BASE_MENU + KEY_GEN_LOGIN;
        case 2:  return FMSG_BASE_MENU + KEY_GEN_BOOKMARKS;
        case 3:  return FMSG_BASE_MENU + KEY_GEN_HISTORY;
        case 5:  return FMSG_BASE_MENU + KEY_MENU;
        }
        return 0;
        
    case ACTION_DOWNLOAD:
    case ACTION_UPLOAD:
        oldmode = site.batch_mode;
        site.batch_mode = TRUE;
        if (action == ACTION_DOWNLOAD) rc = do_get (optarg1);
        if (action == ACTION_UPLOAD) rc = do_put (optarg1);
        //set_view_mode (VIEW_CONTROL);
        //update (1);
        if (rc && !cmdline.batchmode) fly_ask_ok (0, MSG(M_TRANSFER_FAILED), optarg1);
        if ((disc_after && rc == 0) || cmdline.batchmode)
        {
            Logoff ();
            terminate ();
            exit (0);
        }
        site.batch_mode = oldmode;
        return 0;

    case ACTION_CMDLIST:
        rc = runscript (optarg1);
        return 0;

    case ACTION_NICK_BOOK:
    case ACTION_NICK_HIST:
        if (action == ACTION_NICK_BOOK && bookmark_nickname (optarg1, &u) == 0) return 0;
        if (action == ACTION_NICK_HIST && history_nickname (optarg1, &u) == 0) return 0;
        rc = Login (&u);
        if (rc) return 0;
        if (options.login_bell) Bell (3);
        return 0;

    case ACTION_OPEN_BOOKMARKS:
        return FMSG_BASE_MENU + KEY_GEN_BOOKMARKS;
        
    case ACTION_OPEN_HISTORY:
        return FMSG_BASE_MENU + KEY_GEN_HISTORY;

    case ACTION_LOGIN:
        // if download_path was specified in nftp.ini, set it now
        if (options.download_path != NULL)
            set_local_path (options.download_path);
        build_local_filelist (NULL);
        parse_url (optarg1, &u);
        rc = Login (&u);
        // attempt to download file if chdir failed
        /*if (site.set_up && strcmp (site.u.pathname, RCURDIR.name) != 0)
        {
            rc = do_get (optarg1);
        }
        if (rc) return 0;
        */
        if (options.login_bell) Bell (3);
        return 0;

    case ACTION_TESTONLY:
        terminate ();
        exit (0);
    }
    fly_error ("internal error in init()");
    return 0;
}

/* ------------------------------------------------------------- */

void terminate (void)
{
    write_config ();
    video_update (0);
    
    set_window_name (wintitle);
        
    fly_terminate ();
#ifdef __MINGW32__
    WSACleanup ();
#endif
}

/* ------------------------------------------------------------- */

void usage (void)
{
    fprintf (stderr,
             "\n"
             "%s\n"
             "\n"
             "  nftp site-URL\n"
             "      %s\n"
             "\n"
             "  nftp -{G|g|P|p} file-URL\n"
             "      %s\n"
             "\n"
             "  nftp -@ cmdfile.txt\n"
             "      %s\n"
             "\n"
             "  nftp -{b|h} word\n"
             "      %s\n"
             "\n"
             "%s\n"
             "\n",
             MSG(M_USAGE0), MSG(M_USAGE1), MSG(M_USAGE2), MSG(M_USAGE3),
             MSG(M_USAGE4), MSG(M_USAGE5));
}

/* -------------------------------------------------------------------- */

void determine_paths (char *argv0)
{
    char   buffer[8192], buffer2[MAX_PATH], *p;

    if (!fl_opt.has_driveletters)
    {
        // 1. User directory
        p = getenv ("HOME");
        if (p == NULL) fly_error ("HOME variable is not set, aborting\n\n");
        str_scopy (buffer, p);
        str_cats (buffer, ".nftp");
        if (access (buffer, X_OK) < 0)
#ifdef __WIN32__        
            if (mkdir (buffer) < 0)
                fly_error ("\nCannot create directory `%s'\n\n", buffer);
#else
            if (mkdir (buffer, 0700) < 0)
                fly_error ("\nCannot create directory `%s'\n\n", buffer);
#endif
        paths.user_libpath = strdup (buffer);
        chmod (buffer, 0700);

        // 2. System directory
        strcpy (buffer, USR_PATH"/nftp");
        if (access (buffer, X_OK) == 0)
        {
            paths.system_libpath = strdup (buffer);
        }
        else 
        {
            strcpy (buffer, USR_LOCAL_PATH"/nftp");
            if (access (buffer, X_OK) == 0)
               paths.system_libpath = strdup (buffer);
            else
               paths.system_libpath = paths.user_libpath;
        }
    }
    else
    {
        // 1. System directory
        strcpy (buffer, argv0);
        str_translate (buffer, '\\', '/');
        p = strrchr (buffer, '/');
        if (p == NULL) p = buffer;
        *p = '\0';
    
        // bare filename
        if (p == buffer)
        {
            buffer[0] = query_drive () + 'a';
            buffer[1] = ':';
            getcwd (buffer2, sizeof(buffer2));
            str_translate (buffer2, '\\', '/');
            if (buffer2[1] == ':')
                strcpy (buffer+2, buffer2+2);
            else
                strcpy (buffer+2, buffer2);
        }
        paths.system_libpath = strdup (buffer);
        
        // 2. User directory
        paths.user_libpath = paths.system_libpath;
    }
}

/* -------------------------------------------------------------------- */

#define MAX_ENTRIES   1024
#define MAX_SECTIONS  16

typedef struct
{
    int   section;
    char  *variable;
    char  *value;
}
ini_entry;

void update_inifile (char *inifile, char *ifile)
{
    char        *sections[MAX_SECTIONS];
    char        buffer[1024], backupname[1024], *varname, *varvalue;
    ini_entry   *items;
    int         i, current_section, n_sect = 0, n_items = 0;
    FILE        *fp, *fp1, *fp2;

    //if (fl_opt.has_console)
    //    fly_ask_ok (0, MSG(M_INI_UPDATING), inifile, ifile);
    items = (ini_entry *) malloc (sizeof (ini_entry)*MAX_ENTRIES);

    strcpy (buffer, "\n");
    str_strip2 (buffer, " \n\r");

    // open files checking for errors
    fp = fopen (inifile, "r");
    if (fp == NULL) fly_error (MSG(M_INI_CANT_OPEN_READ), inifile);
    fp1 = fopen (ifile, "r");
    if (fp1 == NULL) fly_error (MSG(M_INI_CANT_OPEN_READ), ifile);
    
    // gather updates
    while (fgets (buffer, sizeof (buffer), fp) != NULL)
    {
        str_strip2 (buffer, " \n\r");
        if (strlen(buffer) == 0 || buffer[0] == ';') continue;
        if (buffer[0] == '[')
        {
            sections[n_sect] = strdup (buffer);
            n_sect++;
        }
        else
        {
            if (n_sect == 0)
            {
                fly_error (MSG(M_INI_WRONG_ENTRY), buffer);
            }
            items[n_items].section = n_sect-1;
            if (str_break_ini_line (buffer, &(items[n_items].variable),
                                    &(items[n_items].value)) == 0)
                n_items++;
        }
    }
    fclose (fp);

    // printing for visual check
    //if (fl_opt.has_console)
    //    for (i=0; i<n_items; i++)
    //    {
    //        fprintf (stderr, "%s : %s = %s\n", sections[items[i].section],
    //                 items[i].variable, items[i].value);
    //    }

    // rename nftp.ini into nftp.ini.001, nftp.ini.002 etc.
    for (i=1; ; i++)
    {
        snprintf1 (backupname, sizeof(backupname), "%s.%03u", inifile, i);
        if (access (backupname, F_OK) != 0) break;
    }
    if (rename (inifile, backupname) < 0)
    {
        if (fl_opt.has_console)
            fprintf (stderr, MSG(M_INI_CANT_RENAME), inifile, backupname);
        // assume we're on FAT...
        for (i=1; ; i++)
        {
            snprintf1 (backupname, sizeof(backupname), "nftp.%03u", i);
            if (access (backupname, F_OK) != 0) break;
        }
        if (rename (inifile, backupname) < 0)
            fly_error (MSG(M_INI_CANT_RENAME), inifile, backupname);
    }
    if (fl_opt.has_console)
        fprintf (stderr, MSG(M_INI_BACKUP), inifile, backupname);

    // process .i file and create new ini file
    fp2 = fopen (inifile, "w");
    if (fp2 == NULL) fly_error (MSG(M_INI_CANT_OPEN_WRITE), inifile);

    current_section = -1;
    while (fgets (buffer, sizeof (buffer), fp1) != NULL)
    {
        str_strip2 (buffer, " \n\r");
        switch (buffer[0])
        {
        case '[':
            for (i=0; i<n_sect; i++)
                if (strcmp (sections[i], buffer) == 0) break;
            if (i == n_sect) current_section = -1;
            else             current_section = i;
            break;
        case ';':
            if (str_break_ini_line (buffer+1, &varname, &varvalue) == 0)
            {
                for (i=0; i<n_items; i++)
                    if (items[i].section == current_section &&
                        strcmp (items[i].variable, varname) == 0)
                    {
                        snprintf1 (buffer, sizeof(buffer), "%s = %s", varname, items[i].value);
                        //if (fl_opt.has_console)
                        //    fprintf (stderr, MSG(M_INI_UPDATING_ENTRY), buffer);
                    }
                free (varname);
                free (varvalue);
            }
            break;
        default:
            break;
        }
        fputs (buffer, fp2); fputs ("\n", fp2);
    }
    fclose (fp1);
    fclose (fp2);

    // free arrays
    for (i=0; i<n_sect; i++)
        free (sections[i]);
    for (i=0; i<n_items; i++)
    {
        free (items[i].variable);
        free (items[i].value);
    }
    free (items);

    //if (fl_opt.has_console)
    //{
    //    fprintf (stderr, MSG(M_INI_FINISHED));
    //    fgets (buffer, sizeof(buffer), stdin);
    //}
}

/* -------------------------------------------------------------------- */

void load_menu (void)
{
    char    menu_filename[1024], *language;
    
    // find out what language user wants
    if (options.english_menu || cmdline.english)
    {
        language = "english";
    }
    else
    {
        language = cfg_get_string (CONFIG_NFTP, fl_opt.platform_nick, "language");
        str_strip (language, " ");
    }

Rescan:
    
    // Try $user_libpath directory
    strcpy (menu_filename, paths.user_libpath);
    str_cats (menu_filename, language);
    strcat (menu_filename, ".mnu");
    if (access (menu_filename, R_OK) == 0) goto Found;

    // Try $user_libpath/nls directory
    strcpy (menu_filename, paths.user_libpath);
    str_cats (menu_filename, "nls");
    str_cats (menu_filename, language);
    strcat (menu_filename, ".mnu");
    if (access (menu_filename, R_OK) == 0) goto Found;

    // Try $system_libpath directory
    strcpy (menu_filename, paths.system_libpath);
    str_cats (menu_filename, language);
    strcat (menu_filename, ".mnu");
    if (access (menu_filename, R_OK) == 0) goto Found;

    // Try $system_libpath/nls directory
    strcpy (menu_filename, paths.system_libpath);
    str_cats (menu_filename, "nls");
    str_cats (menu_filename, language);
    strcat (menu_filename, ".mnu");
    if (access (menu_filename, R_OK) == 0) goto Found;

    // Try ../nls
    strcpy (menu_filename, "../nls");
    str_cats (menu_filename, language);
    strcat (menu_filename, ".mnu");
    if (access (menu_filename, R_OK) == 0) goto Found;

    if (strcmp (language, "english") != 0)
    {
        if (fl_opt.has_console && !fl_opt.initialized)
            fly_ask_ok (0, "Failed to load \"%s.mnu\", trying English.\n", language);
        language = "english";
        goto Rescan;
    }
    else
        fly_error ("Failed to load \"%s.mnu\".", language);

Found:

    if (main_menu != NULL)
    {
        menu_unload (main_menu);
    }
    
    if (fl_opt.has_console && !fl_opt.initialized)
        fly_ask_ok (0, "loading %s......\n", menu_filename);
    main_menu = menu_load (menu_filename, options.keytable,
                           sizeof(options.keytable)/sizeof(options.keytable[0]));
}

/* -------------------------------------------------------------------- */

void clear_tmp (void)
{
    char               sample[MAX_PATH], *p;
    DIR                *d;
    struct dirent      *e;
    struct stat        st;
    time_t             now;

    time (&now);
    
    // find tmp files directory
    tmpnam1 (sample);
    p = strrchr (sample, '/');
    *p = '\0';

    // get list of nftp* files in it
    d = opendir (sample);
    if (d == NULL) return;
    *p = '/';

    while (TRUE)
    {
        e = readdir (d);
        if (e == NULL) break;
        if (strcmp (e->d_name, ".") == 0) continue;
        if (strcmp (e->d_name, "..") == 0) continue;
        if (fnmatch1 ("nftp????.???", e->d_name, 0)) continue;
        if (strspn (e->d_name, "nftp0123456789.") != strlen (e->d_name)) continue;
        strcpy (p+1, e->d_name);
        if (stat (sample, &st)) continue;
        if (st.st_mtime > now-7*24*60*60) continue;
        remove (sample);
    }
}

/* -------------------------------------------------------------------- */

void init_config (void)
{
    char    inifile[MAX_PATH], ifile[MAX_PATH];
    char    cfgfile[MAX_PATH];
    int     i;
    
    // set defaults
    cfg_set_string (CONFIG_NFTP, fl_opt.platform_nick, "language", "english");

    // read stored customizations
    strcpy (cfgfile, paths.user_libpath);
    str_cats (cfgfile, "nftp.cfg");
    if (fl_opt.has_console && !fl_opt.initialized)
        fly_ask_ok (0, "loading %s......\n", cfgfile);
    cfg_read (CONFIG_NFTP, cfgfile);

    if (cmdline.language != NULL)
    {
        cfg_set_string (CONFIG_NFTP, fl_opt.platform_nick, "language", cmdline.language);
    }

    // load nftp.ini
    strcpy (inifile, paths.user_libpath);
    str_cats (inifile, "nftp.ini");
    if (fl_opt.has_console && !fl_opt.initialized)
        fly_ask_ok (0, "loading %s......\n", inifile);

    if (access (inifile, F_OK) != 0)
    {
        // looking for nftp.i file in user dir, then in system dir
        strcpy (ifile, paths.user_libpath);
        str_cats (ifile, "nftp.i");
        if (access (ifile, R_OK) != 0)
        {
            strcpy (ifile, paths.system_libpath);
            str_cats (ifile, "nftp.i");
            if (access (ifile, R_OK) != 0)
                fly_error ("Cannot find nftp.i neither in %s nor in %s",
                           paths.user_libpath, paths.system_libpath);
        }
        if (copy_file (ifile, inifile))
            fly_error ("Error copying %s to %s", ifile, inifile);
    }
    
    if (access (inifile, R_OK) != 0)
    {
        fly_error ("Cannot read %s", inifile);
    }

    GetProfileOptions (inifile);

    if (cmdline.slowlink) options.slowlink = TRUE;
    if (options.slowlink) fl_opt.use_ceol = TRUE;

    for (i=0; i<sizeof(runtime_flags)/sizeof(runtime_flags[0]); i++)
    {
        *(runtime_flags[i].value) = runtime_flags[i].def;
        if (runtime_flags[i].name != NULL &&
            cfg_check_boolean (CONFIG_NFTP, NULL, runtime_flags[i].name))
            *(runtime_flags[i].value) = cfg_get_boolean (CONFIG_NFTP, NULL, runtime_flags[i].name);
    }

    check_licensing (paths.user_libpath, paths.system_libpath);
}

/* -------------------------------------------------------------------- */

void write_config (void)
{
    char    cfgfile[MAX_PATH], *p;
    int     rc, x0, y0, i;
    
    rc = video_get_window_pos (&x0, &y0);
    if (rc == 0)
    {
        cfg_set_integer (CONFIG_NFTP, fl_opt.platform_nick, "x0", x0);
        cfg_set_integer (CONFIG_NFTP, fl_opt.platform_nick, "y0", y0);
    }
    
    cfg_set_integer (CONFIG_NFTP, fl_opt.platform_nick, "rows", video_vsize());
    cfg_set_integer (CONFIG_NFTP, fl_opt.platform_nick, "cols", video_hsize());

    for (i=0; i<sizeof(runtime_flags)/sizeof(runtime_flags[0]); i++)
    {
        if (runtime_flags[i].name != NULL)
            cfg_set_boolean (CONFIG_NFTP, NULL, runtime_flags[i].name, *(runtime_flags[i].value));
    }

    p = fly_get_font ();
    cfg_set_string (CONFIG_NFTP, fl_opt.platform_nick, "font", p);
    free (p);

    if (fl_opt.has_driveletters)
    {
        strcpy (cfgfile, "a:/");
        cfgfile[0] = query_drive() + 'a';
        str_cats (cfgfile, local.dir.name);
        cfg_set_string (CONFIG_NFTP, "", "local-directory", cfgfile);
    }
    else
    {
        cfg_set_string (CONFIG_NFTP, "", "local-directory", local.dir.name);
    }

    strcpy (cfgfile, paths.user_libpath);
    str_cats (cfgfile, "nftp.cfg");
    cfg_write (CONFIG_NFTP, cfgfile);
}

/* -------------------------------------------------------------------- */

void config_fly (void)
{
    // reflect command-line options
    if (cmdline.monochrome) options.monochrome = TRUE;
    if (cmdline.colour)     options.monochrome = FALSE;
    
    if (options.monochrome) fly_usemono ();
    fl_opt.menu_hotmouse = options.hotmouse_menu;

    if (options.pseudographics == 0)
    {
        fl_sym.h = nls_strings[M_HBAR][0];
        fl_sym.v = nls_strings[M_VBAR][0];
        fl_sym.x = nls_strings[M_CROSS][0];
        fl_sym.fill1 = nls_strings[M_FILL1][0];
        fl_sym.fill2 = nls_strings[M_FILL2][0];
        fl_sym.fill3 = nls_strings[M_FILL3][0];
        fl_sym.fill4 = nls_strings[M_FILL4][0];
        fl_sym.c_lu = nls_strings[M_LU][0];
        fl_sym.c_ru = nls_strings[M_RU][0];
        fl_sym.c_ld = nls_strings[M_LD][0];
        fl_sym.c_rd = nls_strings[M_RD][0];
        fl_sym.t_l  = nls_strings[M_LT][0];
        fl_sym.t_r  = nls_strings[M_RT][0];
        fl_sym.t_u  = nls_strings[M_UT][0];
        fl_sym.t_d  = nls_strings[M_DT][0];
        fl_sym.placeholder = nls_strings[M_PLACEHOLDER][0];
        fl_sym.marksign    = nls_strings[M_MARKSIGN][0];
    }
    
    // under Unix, replace linedrawing characters
    if (options.pseudographics == 1)
    {
        fl_sym.h        = '-';
        fl_sym.v        = '|';
        fl_sym.x        = '+';
        fl_sym.fill1    = '.';
        fl_sym.fill2    = '.';
        fl_sym.fill3    = 'x';
        fl_sym.fill4    = 'X';
        fl_sym.c_lu     = '/';
        fl_sym.c_ru     = '\\';
        fl_sym.c_ld     = '\\';
        fl_sym.c_rd     = '/';
        fl_sym.t_l      = '>';
        fl_sym.t_r      = '<';
        fl_sym.t_u      = '+';
        fl_sym.t_d      = '+';
        fl_sym.placeholder = ' ';
        fl_sym.marksign    = '*';
    }
    
    if (options.pseudographics == 2)
    {
        fl_sym.h        = '-';
        fl_sym.v        = ' ';
        fl_sym.x        = '-';
        fl_sym.fill1    = '.';
        fl_sym.fill2    = '.';
        fl_sym.fill3    = 'x';
        fl_sym.fill4    = 'X';
        fl_sym.c_lu     = '-';
        fl_sym.c_ru     = '-';
        fl_sym.c_ld     = '-';
        fl_sym.c_rd     = '-';
        fl_sym.t_l      = '-';
        fl_sym.t_r      = '-';
        fl_sym.t_u      = '-';
        fl_sym.t_d      = '-';
        fl_sym.placeholder = ' ';
        fl_sym.marksign    = '*';
    }

    // replace some strings in FLY
    fl_str.yes    = MSG(M_YES);
    fl_str.no     = MSG(M_NO);
    fl_str.ok     = MSG(M_OK);
    fl_str.cancel = MSG(M_CANCEL);
}

/* -------------------------------------------------------------------- */

void update_switches (void)
{
    int  i;
    
    for (i=0; i<sizeof(runtime_flags)/sizeof(runtime_flags[0]); i++)
    {
        menu_set_switch (runtime_flags[i].key, *(runtime_flags[i].value));
    }
}
