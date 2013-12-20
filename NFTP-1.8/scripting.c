#include <fly/fly.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "nftp.h"
#include "net.h"
#include "local.h"
#include "auxprogs.h"
#include "mirror.h"

#define logline(s) \
    { \
    PutLineIntoResp2 (""); \
    PutLineIntoResp2 (M("SCRIPT: %s %s"), s, arg); \
    }

/* ------------------------------------------------------------- */

static int process_script_command (char *command);
static int nargs (char *argstring, int numargs);
static char *extract_arg (char *argstring, int n);
static int checkstop (int rc);

static int script_open (char *arg);
static int script_close (char *arg);
static int script_binary (char *arg);
static int script_ascii (char *arg);
static int script_rename (char *arg);
static int script_lrename (char *arg);
static int script_delete (char *arg);
static int script_ldelete (char *arg);
static int script_cd (char *arg);
static int script_lcd (char *arg);
static int script_reread (char *arg);
static int script_lreread (char *arg);
static int script_mkdir (char *arg);
static int script_lmkdir (char *arg);
static int script_rmdir (char *arg);
static int script_lrmdir (char *arg);
static int script_get (char *arg);
static int script_put (char *arg);
static int script_mget (char *arg);
static int script_mput (char *arg);
static int script_mirror (char *arg);
static int script_set (char *arg);
static int script_logfile (char *arg);
static int script_quit (char *arg);
static int script_quote (char *arg);

/* ------------------------------------------------------------- */

static struct
{
    char *name;
    int (*function)(char *arg);
}
command_table[] =
{
    {"open",    script_open},
    {"close",   script_close},
    {"ascii",   script_ascii},
    {"binary",  script_binary},
    {"rename",  script_rename},
    {"lrename", script_lrename},
    {"delete",  script_delete},
    {"ldelete", script_ldelete},
    {"cd",      script_cd},
    {"lcd",     script_lcd},
    {"reread",  script_reread},
    {"lreread", script_lreread},
    {"mkdir",   script_mkdir},
    {"lmkdir",  script_lmkdir},
    {"rmdir",   script_rmdir},
    {"lrmdir",  script_lrmdir},
    {"quote",   script_quote},
    {"get",     script_get},
    {"put",     script_put},
    {"mget",    script_mget},
    {"mput",    script_mput},
    {"mirror",  script_mirror},
    {"set",     script_set},
    {"logfile", script_logfile},
    {"quit",    script_quit},
};

static struct
{
    char *name;   // scripting variable name
    char *value;  // scripting variable value (all scripting vars are strings)
    int  *gptr;   // pointer to global NFTP var corresponding scripting var (NULL if not used)
    int  backup;  // storage for backup of global NFTP variable
}
internal_vars[] =
{
    {"errorstop",               NULL, NULL,  0},
    {"errorexit",               NULL, NULL,  0},
    {"mirror.delete-unmatched", NULL, NULL,  0},
    {"mirror.include-subdirs",  NULL, NULL,  0},
};

FILE *script_log = NULL;

/* -------------------------------------------------------------
 returns -1 if failed, 0 if OK
 */

int runscript (char *scriptfile)
{
    char    cmd[8192];
    int     rc = 0, oldmode, i;
    FILE    *fp;

    // open script file
    fp = fopen (scriptfile, "r");
    if (fp == NULL)
    {
        PutLineIntoResp2 (M("Cannot open script file '%s'"), scriptfile);
        return -1;
    }

    // modify NFTP global variables
    oldmode = site.batch_mode;
    site.batch_mode = TRUE;
    for (i=0; i<sizeof(internal_vars)/sizeof(internal_vars[0]); i++)
    {
        if (internal_vars[i].gptr != NULL)
        {
            internal_vars[i].backup = *(internal_vars[i].gptr);
        }
    }
    
    while ((fgets (cmd, sizeof(cmd), fp) != NULL))
    {
        str_strip2 (cmd, " \n\r");
        // ignore empty lines and comments
        if (*cmd == '\0' || *cmd == '#') continue;
        str_translate (cmd, '\t', ' ');
        rc = process_script_command (cmd);
        if (rc) break;
    }
    
    if (rc)
    {
        PutLineIntoResp2 (M("Error; script execution has been stopped"));
        if (internal_vars[1].value != NULL && strcmp (internal_vars[1].value, "1") == 0)
        {
            //script_quit ("");
            PutLineIntoResp2 ("");
            PutLineIntoResp2 ("Stopping due to script error");
            terminate ();
            exit (1);
        }
    }

    if (script_log != NULL) fclose (script_log);
    script_log = NULL;
    
    // restore NFTP global variables
    site.batch_mode = oldmode;
    for (i=0; i<sizeof(internal_vars)/sizeof(internal_vars[0]); i++)
    {
        if (internal_vars[i].gptr != NULL)
        {
            *(internal_vars[i].gptr) = internal_vars[i].backup;
        }
    }
    
    return rc;
}

/* -------------------------------------------------------------
 checks supplied error code; if 0 returns 0, if -1 and errorstop
 is "1", returns -1
 */
static int checkstop (int rc)
{
    if (rc == 0) return rc;
    if (internal_vars[0].value == NULL || strcmp (internal_vars[0].value, "1") == 0) return rc;
    return 0;
}

/* ------------------------------------------------------------- */
static int nargs (char *argstring, int numargs)
{
    char   *p;
    int    i, n = 1;

    // count arguments
    p = argstring;
    for (i=0; i<16; i++)
    {
        p = strchr (p, ' ');
        if (p == NULL) break;
        while (*p == ' ') p++;
        n++;
    }

    if (argstring[0] == '\0') n = 0;

    if (n != numargs)
        PutLineIntoResp2 (M("This command requires %d arguments (found %d instead)"), numargs, n);
    return (n != numargs);
}

/* -------------------------------------------------------------
 n is the number of desired argument, starting from 1. malloc()ed
 space is returned
 */
static char *extract_arg (char *argstring, int n)
{
    char   *p, *arg;
    int    i;

    // count arguments
    p = argstring;
    for (i=0; i<n-1; i++)
    {
        p = strchr (p, ' ');
        if (p == NULL) break;
        while (*p == ' ') p++;
    }
    
    if (p == NULL) return strdup ("???"); // hmm...

    arg = strdup (p);
    p = strchr (arg, ' ');
    if (p != NULL) *p = '\0';

    return arg;
}

/* ------------------------------------------------------------- */
static int script_open (char *arg)
{
    url_t u;
    int   rc;
    char  *p;

    //logline ("open"); don't show password!
    PutLineIntoResp2 ("");
    p = strchr (arg, '@');
    if (p != NULL)
    {
        PutLineIntoResp2 (M("SCRIPT: %s %s"), "open", p+1);
    }
    else
    {
        PutLineIntoResp2 (M("SCRIPT: %s %s"), "open", arg);
    }
    if (nargs (arg, 1)) return -1;

    parse_url (arg, &u);
    rc = Login (&u);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_close (char *arg)
{
    logline ("close");
    if (nargs (arg, 0)) return -1;

    Logoff ();
    return 0;
}

/* ------------------------------------------------------------- */
static int script_rename (char *arg)
{
    int   rc, rsp;
    char  *from, *to;
    
    logline ("rename");
    if (nargs (arg, 2)) return -1;

    from = extract_arg (arg, 1);
    to = extract_arg (arg, 2);
    
    rc = SendToServer (TRUE, &rsp, NULL, "RNFR %s", from);
    if (rc || rsp != 3) rc = -1;
    else
    {
        rc = SendToServer (TRUE, &rsp, NULL, "RNTO %s", to);
        if (rc || rsp != 2) rc = -1;
    }

    free (from); free (to);
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_lrename (char *arg)
{
    int   rc;
    char  *from, *to;
    
    logline ("lrename");
    if (nargs (arg, 2)) return -1;

    from = extract_arg (arg, 1);
    to = extract_arg (arg, 2);
    
    rc = rename (from, to);

    free (from); free (to);
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_delete (char *arg)
{
    int i, rc, rsp;
    
    logline ("delete");
    if (nargs (arg, 1)) return -1;

    rc = 0;
    for (i=0; i<RNFILES; i++)
    {
        if (!(RFILE(i).flags & FILE_FLAG_DIR) &&
            fnmatch1 (arg, RFILE(i).name, 0) == 0)
        {
            rc += SendToServer (TRUE, &rsp, NULL, "DELE %s", RFILE(i).name);
            if (rsp != 2) rc += 1;
        }
    }
    retrieve_dir (RCURDIR.name);

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_ldelete (char *arg)
{
    int   i, rc;
    
    logline ("ldelete");
    if (nargs (arg, 1)) return -1;

    build_local_filelist (NULL);
    rc = 0;
    for (i=0; i<LNFILES; i++)
    {
        if (!(LFILE(i).flags & FILE_FLAG_DIR) &&
            fnmatch1 (arg, LFILE(i).name, 0) == 0)
        {
            rc += unlink (LFILE(i).name);
        }
    }
    build_local_filelist (NULL);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_cd (char *arg)
{
    int rc;
    
    logline ("cd");
    if (nargs (arg, 1)) return -1;

    rc = change_dir (arg);
    if (rc != 0) return checkstop (rc);

    if (strcmp (site.u.pathname, RCURDIR.name) != 0)
        rc = set_wd (RCURDIR.name, TRUE);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_binary (char *arg)
{
    int rc;
    
    logline ("binary");
    if (nargs (arg, 0)) return -1;

    status.binary = TRUE;
    rc = SetTransferMode ();

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_ascii (char *arg)
{
    int rc;
    
    logline ("ascii");
    if (nargs (arg, 0)) return -1;

    status.binary = FALSE;
    rc = SetTransferMode ();

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_reread (char *arg)
{
    int rc;
    
    logline ("reread");
    if (nargs (arg, 0)) return -1;

    rc = retrieve_dir (RCURDIR.name);

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_lreread (char *arg)
{
    logline ("lreread");
    if (nargs (arg, 0)) return -1;

    build_local_filelist (arg);

    return checkstop (0);
}

/* ------------------------------------------------------------- */
static int script_lcd (char *arg)
{
    int    newdrive, rc;
    char   buffer[MAX_PATH];
    
    logline ("lcd");
    if (nargs (arg, 1)) return -1;

    // try to set default drive
    if (fl_opt.has_driveletters && arg[1] == ':')
    {
        rc = 0;
        // extract drive from path string
        newdrive = arg[0];
        if (newdrive <= 'Z' && newdrive >= 'A')
        {
            newdrive = newdrive - 'A';
        }
        else
        {
            if (newdrive <= 'z' && newdrive >= 'a')
                newdrive = newdrive - 'a';
            else
                newdrive = 2; /* C: */
        }

        if (newdrive < 26  && newdrive >= 0)
        {
            if (change_drive (newdrive) || getcwd (buffer, sizeof (buffer)) == NULL)
            {
                PutLineIntoResp2 (M("Can't access drive %c:"), newdrive+'A');
                rc = -1;
            }
        }
        local.drive = newdrive;
        if (rc == -1) return checkstop (rc);
    }

    rc = chdir (arg);
    if (rc) PutLineIntoResp2 (M("Directory '%s' does not exist"), arg);

    build_local_filelist (NULL);
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_mkdir (char *arg)
{
    int rc, rsp;
    
    logline ("mkdir");
    if (nargs (arg, 1)) return -1;
    
    rc = SendToServer (TRUE, &rsp, NULL, "MKD %s", arg);
    if (rc || rsp != 2) rc = -1;
    if (rc != 0) return checkstop (rc);

    rc = retrieve_dir (RCURDIR.name);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_lmkdir (char *arg)
{
    int rc;
    
    logline ("lmkdir");
    if (nargs (arg, 1)) return -1;

#ifdef __WIN32__
    rc = mkdir (arg);
#else    
    rc = mkdir (arg, 0777);
#endif   
    build_local_filelist (arg);

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_rmdir (char *arg)
{
    int rc, rsp;
    
    logline ("rmdir");
    if (nargs (arg, 1)) return -1;
    
    rc = SendToServer (TRUE, &rsp, NULL, "RMD %s", arg);
    if (rc || rsp != 2) return -1;
    if (rc != 0) return checkstop (rc);

    rc = retrieve_dir (RCURDIR.name);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_lrmdir (char *arg)
{
    int rc;
    
    logline ("lrmdir");
    if (nargs (arg, 1)) return -1;

    rc = rmdir (arg);
    build_local_filelist (arg);

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_quote (char *arg)
{
    int rc, rsp;
    
    logline ("quote");
    if (arg[0] == '\0') return -1;

    rc = SendToServer (TRUE, &rsp, NULL, "%s", arg);

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_get (char *arg)
{
    trans_item  T;
    int         i, rc;

    logline ("get");
    if (nargs (arg, 1)) return -1;

    // look for specified file
    for (i=0; i<RNFILES; i++)
    {
        if (!(RFILE(i).flags & FILE_FLAG_DIR) && strcmp (RFILE(i).name, arg) == 0) break;
    }

    T.r_dir = RCURDIR.name;
    T.l_dir = local.dir.name;
    T.r_name = arg;
    T.l_name = arg;
    T.mtime  = i == RNFILES ? time(NULL) : RFILE(i).mtime;
    T.size   = i == RNFILES ? 0 : RFILE(i).size;
    T.perm   = i == RNFILES ? 0644 : RFILE(i).rights;
    T.description = i == RNFILES ? NULL : RFILE(i).desc;
    T.f = i == RNFILES ? NULL : &(RFILE(i));
    T.flags = i == RNFILES ? TF_HIDDEN : 0;

    rc = transfer (DOWN, &T, 1, FALSE);
    
    build_local_filelist (NULL);
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_put (char *arg)
{
    trans_item  T;
    int         i, rc;

    logline ("put");
    if (nargs (arg, 1)) return -1;
    
    // look for local file
    build_local_filelist (NULL);
    for (i=0; i<LNFILES; i++)
    {
        if (!(LFILE(i).flags & FILE_FLAG_DIR) && strcmp (LFILE(i).name, arg) == 0) break;
    }
    if (i == LNFILES) return checkstop (-1);

    T.r_dir = RCURDIR.name;
    T.l_dir = local.dir.name;
    T.r_name = LFILE(i).name;
    T.l_name = LFILE(i).name;
    T.mtime  = LFILE(i).mtime;
    T.size   = LFILE(i).size;
    T.perm   = LFILE(i).rights;
    T.description = LFILE(i).desc;
    T.f = &(LFILE(i));
    T.flags = 0;

    rc = transfer (UP, &T, 1, FALSE);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_mget (char *arg)
{
    trans_item  *T;
    int         i, rc, N, Na;

    logline ("mget");
    if (nargs (arg, 1)) return -1;

    // look for specified files
    Na = 64; // wild guess
    T = malloc (sizeof(trans_item) * Na);
    N = 0;
    for (i=0; i<RNFILES; i++)
    {
        if (!(RFILE(i).flags & FILE_FLAG_DIR) &&
            fnmatch1 (arg, RFILE(i).name, 0) == 0)
        {
            if (N == Na-1)
            {
                Na *= 2;
                T = realloc (T, sizeof(trans_item) * Na);
            }
            T[N].r_dir = RCURDIR.name;
            T[N].l_dir = local.dir.name;
            T[N].r_name = RFILE(i).name;
            T[N].l_name = RFILE(i).name;
            T[N].mtime  = RFILE(i).mtime;
            T[N].size   = RFILE(i).size;
            T[N].perm   = RFILE(i).rights;
            T[N].description = RFILE(i).desc;
            T[N].f = &(RFILE(i));
            T[N].flags = 0;
            N++;
        }
    }

    if (N == 0)
    {
        free (T);
        return 0;
    }

    rc = transfer (DOWN, T, N, FALSE);
    
    build_local_filelist (NULL);
    free (T);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_mput (char *arg)
{
    trans_item  *T;
    int         i, rc, N, Na;

    logline ("mput");
    if (nargs (arg, 1)) return -1;
    
    build_local_filelist (NULL);
    
    // look for specified files
    Na = 64; // wild guess
    T = malloc (sizeof(trans_item) * Na);
    N = 0;
    for (i=0; i<LNFILES; i++)
    {
        if (!(LFILE(i).flags & FILE_FLAG_DIR) &&
            fnmatch1 (arg, LFILE(i).name, 0) == 0)
        {
            if (N == Na-1)
            {
                Na *= 2;
                T = realloc (T, sizeof(trans_item) * Na);
            }
            T[N].r_dir = RCURDIR.name;
            T[N].l_dir = local.dir.name;
            T[N].r_name = LFILE(i).name;
            T[N].l_name = LFILE(i).name;
            T[N].mtime  = LFILE(i).mtime;
            T[N].size   = LFILE(i).size;
            T[N].perm   = LFILE(i).rights;
            T[N].description = LFILE(i).desc;
            T[N].f = &(LFILE(i));
            T[N].flags = 0;
            N++;
        }
    }

    if (N == 0)
    {
        free (T);
        return 0;
    }
    
    rc = transfer (UP, T, N, FALSE);
    
    free (T);
    
    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_mirror (char *arg)
{
    int         rc, d;
    mirror_options_t mo;

    logline ("mirror");
    if (nargs (arg, 1)) return -1;

    // check argument
    if (strcmp (arg, "download") == 0)
    {
        d = LOCAL_WITH_REMOTE;
    }
    else if (strcmp (arg, "upload") == 0)
    {
        d = REMOTE_WITH_LOCAL;
    }
    else
    {
        PutLineIntoResp2 ("%s: invalid argument for mirror", arg);
        return checkstop (-1);
    }
    
    build_local_filelist (NULL);
    get_mirror_options (&mo);
    mo.delete_unmatched = atoi (internal_vars[2].value);
    mo.recurse_subdirs = atoi (internal_vars[3].value);
    rc = mirror (d, FALSE, &mo);

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_set (char *arg)
{
    int   i, rc;
    char  *var_name, *var_value;
    
    logline ("set");
    if (nargs (arg, 2)) return -1;

    var_name  = extract_arg (arg, 1);
    var_value = extract_arg (arg, 2);

    rc = -1;
    for (i=0; i<sizeof(internal_vars)/sizeof(internal_vars[0]); i++)
    {
        if (strcmp (var_name, internal_vars[i].name) == 0)
        {
            if (internal_vars[i].value != NULL) free (var_value);
            internal_vars[i].value = strdup (var_value);
            rc = 0;
            break;
        }
    }
    
    if (rc == -1) PutLineIntoResp2 (M("Invalid variable: %s"), var_name);

    // set the values of the global NFTP variables corresponding
    // to scripting variables
    for (i=0; i<sizeof(internal_vars)/sizeof(internal_vars[0]); i++)
    {
        if (internal_vars[i].gptr != NULL && internal_vars[i].value != NULL)
        {
            *(internal_vars[i].gptr) = atoi (internal_vars[i].value);
        }
    }

    free (var_name); free (var_value);
    return rc;
}

/* ------------------------------------------------------------- */
static int script_logfile (char *arg)
{
    int   rc;

    logline ("logfile");
    if (nargs (arg, 1)) return -1;

    if (script_log != NULL) fclose (script_log);
    rc = 0;
    if (strcmp (arg, "none") != 0)
    {
        script_log = fopen (arg, "a+");
        if (script_log == NULL)
        {
            rc = -1;
            PutLineIntoResp2 (M("Cannot open logfile '%s'"), arg);
        }
        else
        {
            fprintf (script_log, "\n-------- %s --------------------\n",
                     datetime());
        }
    }
    else
        script_log = NULL;

    return checkstop (rc);
}

/* ------------------------------------------------------------- */
static int script_quit (char *arg)
{
    logline ("quit");
    if (nargs (arg, 0)) return -1;

    terminate ();
    exit (0);
    
    return 0;
}

/* -------------------------------------------------------------
 returns -1 if failed, 0 if OK
 */

int process_script_command (char *command)
{
    int    i;
    char   *p, *cmdname, *cmdarg;

    // filter out the command name
    p = strchr (command, ' ');
    if (p == NULL)
    {
        cmdname = command;
        cmdarg = "";
    }
    else
    {
        cmdname = command;
        *p = '\0';
        cmdarg = p+1;
        str_strip2 (cmdarg, " ");
    }
    dmsg ("executing command [%s] with argument [%s]\n", cmdname, cmdarg);

    for (i=0; i<sizeof(command_table)/sizeof(command_table[0]); i++)
    {
        if (strcmp (command_table[i].name, cmdname) == 0)
            return (command_table[i].function)(cmdarg);
    }

    PutLineIntoResp2 (M("Unrecognized command: %s"), cmdname);
    return -1;
}
