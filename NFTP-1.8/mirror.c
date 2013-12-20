#include <fly/fly.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "nftp.h"
#include "net.h"
#include "local.h"
#include "auxprogs.h"

/* ------------------------------------------------------------- */

enum
{
    ACTION_TRANSFER=1,
    ACTION_DELETE=2
};

typedef struct 
{
    char     *path; /* malloc()ed */
    int      isdir;
    int64_t  size;
    time_t   mtime;
}
del_item_t;

static int match1 (int D, int F, int act, mirror_options_t *mo);
static int match2 (char *D, int act, mirror_options_t *mo);
static void drawline_del_down (int n, int len, int shift, int pointer, int row, int col);
static void drawline_del_up (int n, int len, int shift, int pointer, int row, int col);
static int display_transfer_list (int direction);
static int display_delete_list (int direction);
static void drawline_transfer_down (int n, int len, int shift, int pointer, int row, int col);
static void drawline_transfer_up (int n, int len, int shift, int pointer, int row, int col);
static int ex_fname (char *fn, mirror_options_t *mo);
static int ex_dname (char *fn, mirror_options_t *mo);
static void compile_re (mirror_options_t *mo);

static char         *sync_l_root, *sync_r_root;
static trans_item   *Tm;
static int          ntm, ntma;

/* list of files/directories to delete. these are absolute and
 include sync_*_root */
static del_item_t   *DI;
static int          ndi, ndia, tot_files, tot_dirs;

/* ------------------------------------------------------------- */

int mirror (int d, int noisy, mirror_options_t *mo)
{
    int  prev_status, prev_mode, prev_backups, i, rc, delfile, deldir;
    int64_t delsize;
    char *p;

    dmsg ("entered mirror(%d)\n", d);
    sync_r_root = strdup (RCURDIR.name);
    sync_l_root = strdup (local.dir.name);
    dmsg ("sync_r_root=%s, sync_l_root=%s\n", sync_r_root, sync_l_root);
    ntma = 16;
    ntm = 0;
    Tm = xmalloc (sizeof (Tm[0]) * ntma);
    ndia = 16;
    ndi = 0;
    DI = xmalloc (sizeof(del_item_t) * ndia);
    tot_files = 0;
    tot_dirs = 0;
    delfile = 0;
    deldir = 0;

    // switch symlink resolving off
    prev_status = status.resolve_symlinks;
    prev_mode = site.batch_mode;
    prev_backups = options.backups;
    site.batch_mode = TRUE;
    status.resolve_symlinks = FALSE;
    options.backups = FALSE;
    adjust_menu_status ();

    // first we need to scan remote tree -- if needed, of course
    if (mo->recurse_subdirs)
    {
        for (i=0; i<RNFILES; i++)
        {
            if (RFILE(i).flags & FILE_FLAG_DIR)
            {
                if (strcmp (RFILE(i).name, ".") == 0 ||
                    strcmp (RFILE(i).name, "..") == 0) continue;

                if (ex_dname (RFILE(i).name, mo)) continue;

                //traverse_directory (RFILE(i).name, 0);
                rc = cache_subtree (site.cdir, i);
                if (rc == ERR_FATAL || rc == ERR_CANCELLED) return rc;
            }
        }
    }

    if (d == LOCAL_WITH_REMOTE)
    {
        // now we walk the tree and compile the transfer list
        for (i=0; i<site.dir[site.cdir].nfiles; i++)
        {
            if (strcmp (site.dir[site.cdir].files[i].name, "..") == 0) continue;

            match1 (site.cdir, i, ACTION_TRANSFER, mo);
        }

        if (ntm != 0)
        {
            if (mo->dry_run)
            {
                display_transfer_list (DOWN);
            }
            else
            {
                dmsg ("doing mirroring transfer\n");
                rc = transfer (DOWN, Tm, ntm, noisy);
                dmsg ("done mirroring transfer; rc=%d\n", rc);
                if (rc) return rc;
            }
        }

        /* remove unmatched files if needed */
        if (mo->delete_unmatched)
        {
            put_message (M("Collecting list of files/directories to delete..."));
            match2 (sync_l_root, ACTION_DELETE, mo);

            if (ndi != 0)
            {
                if (mo->dry_run)
                {
                    display_delete_list (DOWN);
                }
                else
                {
                    delsize = 0;
                    for (i=0; i<ndi; i++)
                    {
                        if (!DI[i].isdir)
                        {
                            delsize += DI[i].size;
                            delfile++;
                        }
                        else
                            deldir++;
                    }
                    if (mo->confirm_deletions)
                    {
                        fly_ask_ok (ASK_WARN,
                                    M(" %d files, %d directories, total size %s bytes \n"
                                      " are going to be DELETED from LOCAL DISK. \n"
                                      " Please review list of files and confirm deletion. \n"
                                      " Press Enter to see list of files "),
                                    delfile, deldir, pretty64(delsize));
                        /* we ask user when running in interactive mode */
                        rc = fly_choose (M(" Press Enter to delete unmatched files, Esc to cancel "),
                                         CHOOSE_ALLOW_HSCROLL, &ndi, 0,
                                         drawline_del_down, NULL);
                    }
                    else
                    {
                        rc = 0;
                        delfile = tot_files == 0 ? 100 : delfile*100/tot_files;
                        deldir = tot_dirs == 0 ? 100 : deldir*100/tot_dirs;
                        /* in noninteractive mode we refuse to delete files
                         if the percentage of files that would be deleted
                         is greater than value of options.delfile_limit or
                         the percentage of directories that would be deleted
                         is greater than value of options.deldir_limit */
                        if (delfile > options.delfile_limit)
                        {
                            PutLineIntoResp2 (M("Too many files to delete (%d%% > %d%%), NOT deleting"),
                                              delfile, options.delfile_limit);
                            rc = -1;
                        }
                        if (deldir > options.deldir_limit)
                        {
                            PutLineIntoResp2 (M("Too many directories to delete (%d%% > %d%%), NOT deleting"),
                                              deldir, options.deldir_limit);
                            rc = -1;
                        }
                    }
                    if (rc >= 0)
                    {
                        for (i=0; i<ndi; i++)
                        {
                            dmsg ("deleting [%s]\n", DI[i].path);
                            if (DI[i].isdir)
                                rmdir (DI[i].path);
                            else
                                unlink (DI[i].path);
                        }
                    }
                }
                dmsg ("done delete\n");
            }
        }

        /* issue message if nothing has to be done */
        if (ntm == 0 && ndi == 0)
        {
            PutLineIntoResp2 (M("Directories seem to be identical"));
            if (noisy)
                fly_ask_ok (0, M("Directories seem to be identical"));
        }
    }

    else if (d == REMOTE_WITH_LOCAL)
    {
        put_message (M("Scanning local directory..."));
        match2 (sync_l_root, ACTION_TRANSFER, mo);

        if (ntm != 0)
        {
            if (mo->dry_run)
            {
                display_transfer_list (UP);
            }
            else
            {
                rc = transfer (UP, Tm, ntm, noisy);
                if (rc) return rc;
            }
        }

        // remove unmatched files if needed
        if (mo->delete_unmatched)
        {
            put_message (M("Collecting list of remote files/directories to delete..."));
            for (i=0; i<site.dir[site.cdir].nfiles; i++)
            {
                if (strcmp (site.dir[site.cdir].files[i].name, "..") == 0) continue;
                match1 (site.cdir, i, ACTION_DELETE, mo);
            }

            dmsg ("%d Files/directories to delete:\n", ndi);
            for (i=0; i<ndi; i++)
                dmsg ("%d. %s (%s) %s\n", i+1, DI[i].path,
                      DI[i].isdir ? "dir" : "file", pretty64(DI[i].size));

            if (ndi != 0)
            {
                if (mo->dry_run)
                {
                    display_delete_list (UP);
                }
                else
                {
                    delsize = 0;
                    for (i=0; i<ndi; i++)
                    {
                        if (!DI[i].isdir)
                        {
                            delsize += DI[i].size;
                            delfile++;
                        }
                        else
                            deldir++;
                    }
                    dmsg ("delsize=%s, delfile=%d, deldir=%d\n",
                          pretty64(delsize), delfile, deldir);
                    if (mo->confirm_deletions)
                    {
                        fly_ask_ok (ASK_WARN,
                                    " %d files, %d directories, total size %s bytes \n"
                                    " are going to be DELETED from REMOTE SITE. \n"
                                    " Please review list of files and confirm deletion. \n"
                                    " Press Enter to see list of files ",
                                    delfile, deldir, pretty64(delsize));
                        /* we ask user when running in interactive mode */
                        rc = fly_choose (" Press Enter to delete unmatched files, Esc to cancel ",
                                         CHOOSE_ALLOW_HSCROLL, &ndi, 0,
                                         drawline_del_up, NULL);
                    }
                    else
                    {
                        dmsg ("we are not in noisy mode\n");
                        rc = 0;
                        delfile = delfile == 0 ? 0 :
                            (tot_files == 0 ? 100 : delfile*100/tot_files);
                        deldir = deldir == 0 ? 0 :
                            (tot_dirs == 0 ? 100 : deldir*100/tot_dirs);
                        /* in noninteractive mode we refuse to delete files
                         if the percentage of files that would be deleted
                         is greater than value of options.delfile_limit or
                         the percentage of directories that would be deleted
                         is greater than value of options.deldir_limit */
                        if (delfile > options.delfile_limit)
                        {
                            PutLineIntoResp2 ("Too many files to delete (%d%% > %d%%), NOT deleting",
                                              delfile, options.delfile_limit);
                            rc = -1;
                        }
                        if (deldir > options.deldir_limit)
                        {
                            PutLineIntoResp2 ("Too many directories to delete (%d%% > %d%%), NOT deleting",
                                              deldir, options.deldir_limit);
                            rc = -1;
                        }
                    }

                    dmsg ("rc=%d\n", rc);
                    if (rc >= 0)
                    {
                        for (i=0; i<ndi; i++)
                        {
                            p = strrchr (DI[i].path, '/');
                            if (p == NULL) continue;
                            *p = '\0'; p++;
                            if (set_wd (DI[i].path, FALSE) != 0) continue;
                            rc = SendToServer (TRUE, NULL, NULL, "DELE %s", p);
                            if (rc == ERR_CANCELLED) break;
                        }
                    }
                }
                dmsg ("done delete\n");
            }
        }

        if (ntm == 0 && ndi == 0)
        {
            PutLineIntoResp2 (M("Directories seem to be identical"));
            if (noisy)
                fly_ask_ok (0, M("Directories seem to be identical"));
        }
    }

    change_dir (sync_r_root);
    build_local_filelist (NULL);
    
    // switch symlink resolving back to original setting
    status.resolve_symlinks = prev_status;
    adjust_menu_status ();
    site.batch_mode = prev_mode;
    options.backups = prev_backups;
    
    /* invalidate directories on server altered by transfer/delete */
    if (d == REMOTE_WITH_LOCAL && !mo->dry_run)
    {
        char *p;

        /* invalidate all touched r_dir */
        for (i=0; i<ntm; i++)
            invalidate_directory (Tm[i].r_dir, FALSE);
        /* invalidate deleted directories and directories contained deleted files */
        for (i=0; i<ndi; i++)
        {
            if (DI[i].isdir)
            {
                invalidate_directory (DI[i].path, FALSE);
            }
            else
            {
                p = strrchr (DI[i].path, '/');
                if (p != NULL)
                {
                    *p = '\0';
                    invalidate_directory (DI[i].path, FALSE);
                    *p = '/';
                }
            }
        }
    }

    free (sync_r_root);
    free (sync_l_root);
    for (i=0; i<ntm; i++)
    {
        free (Tm[i].r_dir);
        free (Tm[i].r_name);
        free (Tm[i].l_dir);
        free (Tm[i].l_name);
    }
    free (Tm);
    for (i=0; i<ndi; i++)
    {
        free (DI[i].path);
    }
    free (DI);
    dmsg ("returning from mirror()\n");
    return 0;
}

/* ------------------------------------------------------------------ */

/* returns TRUE when transfer is needed due to time difference */
int cmp_timestamp (time_t from, time_t to, mirror_options_t *mo)
{
    time_t dt = to - from;

    dmsg ("from mtime = %u, to mtime = %u, dt = %u\n", from, to, dt);

    if (mo->ignore_timestamps) return FALSE;
    if (abs(dt) < mo->time_jitter) return FALSE;

    /* dt >= 0 means to >= from, i.e. target (to) is older than source
     (from). if target is older than source, no transfer is needed */
    if (dt >= 0) return FALSE;

    return TRUE;
}

/* ------------------------------------------------------------------ */

/* returns TRUE when filename is excluded */
static int ex_fname (char *fn, mirror_options_t *mo)
{
    int i;

    if (mo->f_fnm_valid)
    {
        for (i=0; i<mo->nf_fnm; i++)
        {
            if (fnmatch1 (mo->f_fnm[i], fn,
                          mo->case_insensitive ? FNM1_CASEFOLD : 0) == 0)
                return TRUE;
        }
    }

    if (mo->f_re_valid)
    {
        if (regexec (&mo->f_re, fn, 0, NULL, 0) == 0) return TRUE;
    }

    return FALSE;
}

/* ------------------------------------------------------------------ */

/* returns TRUE when directory name is excluded */
static int ex_dname (char *fn, mirror_options_t *mo)
{
    int i;

    if (mo->d_fnm_valid)
    {
        for (i=0; i<mo->nd_fnm; i++)
        {
            if (fnmatch1 (mo->d_fnm[i], fn,
                          mo->case_insensitive ? FNM1_CASEFOLD : 0) == 0)
                return TRUE;
        }
    }

    if (mo->d_re_valid)
    {
        if (regexec (&mo->d_re, fn, 0, NULL, 0) == 0) return TRUE;
    }

    return FALSE;
}

/* ------------------------------------------------------------------ */

/* compare remote directories/files with local (downloading or deleting
 remote files) */
static int match1 (int D, int F, int act, mirror_options_t *mo)
{
    char          *p, *p1;
    int           d, i, rc;
    struct stat   st;

    dmsg ("entering match1(%d): [%s] in [%s]; size=%s, mtime=%lu\n",
          act, site.dir[D].files[F].name, site.dir[D].name, 
          pretty64(site.dir[D].files[F].size), site.dir[D].files[F].mtime);

    // see if remote is a directory. if it is, change to it and
    // walk it
    if (site.dir[D].files[F].flags & FILE_FLAG_DIR)
    {
        // ignore dirs if 'subdirs' mode is not set
        if (!mo->recurse_subdirs) return 0;

        if (strcmp (site.dir[D].files[F].name, "..") == 0 ||
            strcmp (site.dir[D].files[F].name, ".") == 0) return 0;
        if (ex_dname (site.dir[D].files[F].name, mo)) return 0;

        tot_dirs++;
        dmsg ("counting dir %s/%s\n", site.dir[D].name, site.dir[D].files[F].name);
        // make its name
        p = str_strdup1 (site.dir[D].name, strlen(site.dir[D].files[F].name)+2);
        str_cats (p, site.dir[D].files[F].name);
        // find it in the cache
        for (d=0; d<site.ndir; d++)
        {
            if (mo->case_insensitive)
            {
                if (stricmp1 (p, site.dir[d].name) == 0) break;
            }
            else
            {
                if (strcmp (p, site.dir[d].name) == 0) break;
            }
        }
        free (p);
        if (d == site.ndir) return 0; // not cached; ignore it

        /* create a corresponding local directory. this is necessary
         to have empty directories mirrored */
        if (act == ACTION_TRANSFER)
        {
            /*
            p = site.dir[D].name + strlen (sync_r_root);
            p1 = str_strdup1 (sync_l_root, strlen(p)+2+
                              strlen(site.dir[D].files[F].name));
            str_cats (p1, p);
            str_cats (p1, site.dir[D].files[F].name);
            mkdir (p1, 0777);
            free (p1);
            */

            if (ntm == ntma-1)
            {
                ntma *= 2;
                Tm = realloc (Tm, sizeof (Tm[0]) * ntma);
            }
            
            Tm[ntm].r_dir = strdup (site.dir[D].name);
            Tm[ntm].r_name = strdup (site.dir[D].files[F].name);
            Tm[ntm].l_dir = str_sjoin (sync_l_root, site.dir[D].name + strlen (sync_r_root));
                //str_strdup1 (sync_l_root, strlen(p)+2);
            //str_cats (Tm[ntm].l_dir, p);
            Tm[ntm].l_name = strdup (site.dir[D].files[F].name);
            Tm[ntm].mtime  = site.dir[D].files[F].mtime;
            Tm[ntm].size   = 0;
            Tm[ntm].perm   = site.dir[D].files[F].rights;
            Tm[ntm].description = site.dir[D].files[F].desc;
            //Tm[ntm].f = site.dir[D].files + F;
            Tm[ntm].f = NULL;
            Tm[ntm].flags = TF_DIRECTORY;
            ntm++;
        }

        /* walk it */
        for (i=0; i<site.dir[d].nfiles; i++)
        {
            match1 (d, i, act, mo);
        }
        if (act == ACTION_DELETE)
        {
            // make local path corresponding to that remote directory
            if (str_headcmp (site.dir[D].name, sync_r_root) != 0) return 0;
            p = site.dir[D].name + strlen (sync_r_root);
            p1 = str_strdup1 (sync_l_root, strlen(p)+2+strlen(site.dir[D].files[F].name));
            str_cats (p1, p);
            str_cats (p1, site.dir[D].files[F].name);
            if (access (p1, F_OK) < 0 || stat (p1, &st) !=0 || !S_ISDIR(st.st_mode))
            {
                if (ndi == ndia)
                {
                    ndia *= 2;
                    DI = xrealloc (DI, sizeof(del_item_t) * ndia);
                }
                DI[ndi].path = str_strdup1 (site.dir[D].name, strlen(site.dir[D].files[F].name)+1);
                str_cats (DI[ndi].path, site.dir[D].files[F].name);
                DI[ndi].isdir = TRUE;
                DI[ndi].size = site.dir[D].files[F].size;
                DI[ndi].mtime = site.dir[D].files[F].mtime;
                dmsg ("adding directory %s to delete list\n", DI[ndi].path);
                ndi++;
                //set_wd (site.dir[D].name, FALSE);
                //SendToServer (TRUE, NULL, NULL, "RMD %s", site.dir[D].files[F].name);
            }
            free (p1);
        }
    }

    // remote is a symbolic link?
    else if (site.dir[D].files[F].flags & FILE_FLAG_LINK)
    {
    }

    // remote is a regular file?
    else if (site.dir[D].files[F].flags & FILE_FLAG_REGULAR)
    {
        // first we determine the subdirectory
        if (str_headcmp (site.dir[D].name, sync_r_root) != 0) return 0;

        if (ex_fname (site.dir[D].files[F].name, mo)) return 0;

        tot_files++;
        dmsg ("counting file %s/%s\n", site.dir[D].name, site.dir[D].files[F].name);
        p = site.dir[D].name + strlen (sync_r_root);
        p1 = str_strdup1 (sync_l_root, strlen(p)+2+strlen(site.dir[D].files[F].name));
        str_cats (p1, p);
        str_cats (p1, site.dir[D].files[F].name);
        // p1 now holds corresponding local path
        dmsg ("regular file with local pathname [%s]\n", p1);

        if (act == ACTION_TRANSFER)
        {
            rc = stat (p1, &st);
            free (p1);
            if (rc < 0) goto get_it;
            if (st.st_size != site.dir[D].files[F].size) goto get_it;

            if (cmp_timestamp (site.dir[D].files[F].mtime, st.st_mtime, mo)) goto get_it;

            dmsg ("1. exiting match1\n");
            return 0;
            
        get_it:
            dmsg ("need to get %s/%s as %s\n", site.dir[D].name, site.dir[D].files[F].name, p1);
    
            if (ntm == ntma-1)
            {
                ntma *= 2;
                Tm = realloc (Tm, sizeof (Tm[0]) * ntma);
            }
            
            Tm[ntm].r_dir = strdup (site.dir[D].name);
            Tm[ntm].r_name = strdup (site.dir[D].files[F].name);
            Tm[ntm].l_dir = str_sjoin (sync_l_root, site.dir[D].name + strlen (sync_r_root));
                //str_strdup1 (sync_l_root, strlen(p)+2);
            //str_cats (Tm[ntm].l_dir, p);
            Tm[ntm].l_name = strdup (site.dir[D].files[F].name);
            Tm[ntm].mtime  = site.dir[D].files[F].mtime;
            Tm[ntm].size   = site.dir[D].files[F].size;
            Tm[ntm].perm   = site.dir[D].files[F].rights;
            Tm[ntm].description = site.dir[D].files[F].desc;
            //Tm[ntm].f = site.dir[D].files+F;
            Tm[ntm].f = NULL;
            Tm[ntm].flags = 0;
            ntm++;
        }
        else
        {
            if (access (p1, F_OK) < 0 || stat (p1, &st) || !S_ISREG(st.st_mode))
            {
                /*
                dmsg ("deleting remote: [%s] in [%s]\n", site.dir[D].files[F].name, site.dir[D].name);
                set_wd (site.dir[D].name, FALSE);
                SendToServer (TRUE, NULL, NULL, "DELE %s", site.dir[D].files[F].name);
                */
                if (ndi == ndia)
                {
                    ndia *= 2;
                    DI = xrealloc (DI, sizeof(del_item_t) * ndia);
                }
                DI[ndi].path = str_strdup1 (site.dir[D].name, strlen(site.dir[D].files[F].name)+1);
                str_cats (DI[ndi].path, site.dir[D].files[F].name);
                DI[ndi].isdir = FALSE;
                DI[ndi].size = site.dir[D].files[F].size;
                DI[ndi].mtime = site.dir[D].files[F].mtime;
                dmsg ("adding file %s to delete list\n", DI[ndi].path);
                ndi++;
            }
        }
    }
    
    dmsg ("2. exiting match1\n");
    return 0;
}

/* compare local directories/files with remote. D is local directory to start */
static int match2 (char *D, int act, mirror_options_t *mo)
{
    DIR            *df;
    struct dirent  *e;
    struct stat    st;
    char           *p, *p1, *p2;
    int            i, rd, rf;

    dmsg ("entering match2(%d): [%s]\n", act, D);
    // see if corresponding remote directory exists
    if (str_headcmp (D, sync_l_root) != 0) return 0;
    p = D + strlen (sync_l_root);
    p1 = str_strdup1 (sync_r_root, strlen(p)+2);
    str_cats (p1, p);
    str_strip (p1, "/");
    dmsg ("looking up [%s] on remote\n", p1);
    for (i=0; i<site.ndir; i++)
    {
        if (mo->case_insensitive)
        {
            if (stricmp1 (site.dir[i].name, p1) == 0) break;
        }
        else
        {
            if (strcmp (site.dir[i].name, p1) == 0) break;
        }
    }
    if (i == site.ndir) rd = -1;
    else                rd = i;
    dmsg ("found: %d\n", rd);

    /* scan local directory, look up remote equivalents */
    df = opendir (D);
    if (df == NULL)
    {
        free (p1);
        return -1;
    }
    while (TRUE)
    {
        e = readdir (df);
        if (e == NULL) break;
        if (strcmp (e->d_name, ".")  == 0 || strcmp (e->d_name, "..") == 0) continue;
        
        // make full name and stat() it
        p = str_strdup1 (D, strlen(e->d_name)+2);
        str_cats (p, e->d_name);
        if (stat (p, &st) < 0) goto skip_it;

        if (!mo->recurse_subdirs && S_ISDIR(st.st_mode)) goto skip_it;

        if (S_ISDIR(st.st_mode)) tot_dirs++;
        else if (S_ISREG(st.st_mode)) tot_files++;
        dmsg ("trying to match %s, size=%s, mtime=%lu\n",
              p, pretty64(st.st_size), st.st_mtime);
        // try to locate remote equivalent if possible
        rf = -1;
        if (rd >= 0)
        {
            for (i=0; i<site.dir[rd].nfiles; i++)
            {
                if (mo->case_insensitive)
                {
                    if (stricmp1 (site.dir[rd].files[i].name, e->d_name) == 0 &&
                        ( ((site.dir[rd].files[i].flags & FILE_FLAG_DIR) &&
                           S_ISDIR(st.st_mode)) ||
                          ((site.dir[rd].files[i].flags & FILE_FLAG_REGULAR) &&
                           S_ISREG(st.st_mode)) )) break;
                }
                else
                {
                    if (strcmp (site.dir[rd].files[i].name, e->d_name) == 0 &&
                        ( ((site.dir[rd].files[i].flags & FILE_FLAG_DIR) &&
                          S_ISDIR(st.st_mode)) ||
                          ((site.dir[rd].files[i].flags & FILE_FLAG_REGULAR) &&
                           S_ISREG(st.st_mode)) )) break;
                }
            }
            if (i != site.dir[rd].nfiles) rf = i;
        }
        if (rf == -1)
            dmsg ("not found\n");
        else
            dmsg ("located: [%s], size=%s, mtime=%lu\n",
                  site.dir[rd].files[rf].name,
                  pretty64(site.dir[rd].files[rf].size),
                  site.dir[rd].files[rf].mtime);
        
        // by type
        if (S_ISDIR (st.st_mode))
        {
            if (ex_dname (e->d_name, mo)) goto skip_it;

            // directory
            match2 (p, act, mo);
            if (rf == -1) goto put_it;
        }
        else if (S_ISLNK (st.st_mode))
        {
            // symlinks are ignored for now
            goto skip_it;
        }
        else if (S_ISREG (st.st_mode))
        {
            if (ex_fname (e->d_name, mo)) goto skip_it;

            // regular files
            if (rf == -1) goto put_it;

            /* do not cause deletings if remote and local files differ! */
            if (act == ACTION_DELETE) goto skip_it;

            if (st.st_size != site.dir[rd].files[rf].size) goto put_it;

            /* upload when local file is newer than remote */
            /*if (st.st_mtime > site.dir[rd].files[rf].mtime) goto put_it;*/
            if (cmp_timestamp (st.st_mtime,
                               site.dir[rd].files[rf].mtime, mo)) goto put_it;

        }
        goto skip_it;
        
    put_it:
        if (act == ACTION_TRANSFER)
        {
            if (S_ISDIR (st.st_mode))
            {
                /* create directory on remote */
                /*
                p2 = str_strdup1 (p1, strlen(e->d_name)+1);
                str_cats (p2, e->d_name);
                create_dir (p2);
                free (p2);
                */
                if (ntm == ntma-1)
                {
                    ntma *= 2;
                    Tm = realloc (Tm, sizeof (Tm[0]) * ntma);
                }
                Tm[ntm].r_dir = strdup (p1);
                Tm[ntm].r_name = strdup (e->d_name);
                Tm[ntm].l_dir = strdup (D);
                Tm[ntm].l_name = strdup (Tm[ntm].r_name);
                Tm[ntm].mtime  = st.st_mtime;
                Tm[ntm].size   = 0;
                Tm[ntm].perm   = st.st_mode;
                Tm[ntm].description = NULL;
                Tm[ntm].f = NULL;
                Tm[ntm].flags = TF_DIRECTORY;
                ntm++;
            }
            else if (S_ISREG (st.st_mode))
            {
                /* queue file to upload */
                dmsg ("need to put %s as %s/%s\n", p, p1, e->d_name);
                if (ntm == ntma-1)
                {
                    ntma *= 2;
                    Tm = realloc (Tm, sizeof (Tm[0]) * ntma);
                }
                Tm[ntm].r_dir = strdup (p1);
                Tm[ntm].r_name = strdup (e->d_name);
                Tm[ntm].l_dir = strdup (D);
                Tm[ntm].l_name = strdup (Tm[ntm].r_name);
                Tm[ntm].mtime  = st.st_mtime;
                Tm[ntm].size   = st.st_size;
                Tm[ntm].perm   = st.st_mode;
                Tm[ntm].description = NULL;
                Tm[ntm].f = NULL;
                Tm[ntm].flags = 0;
                ntm++;
            }
        }
        else
        {
            p2 = str_strdup1 (D, strlen(e->d_name)+1);
            str_cats (p2, e->d_name);
            /*
            dmsg ("deleting [%s]\n", p2);
            if (S_ISDIR (st.st_mode))
                rmdir (p2);
            else
                unlink (p2);
                free (p2);
            */
            if (ndi == ndia)
            {
                ndia *= 2;
                DI = xrealloc (DI, sizeof(del_item_t) * ndia);
            }
            DI[ndi].path = p2;
            if (S_ISDIR (st.st_mode)) DI[ndi].isdir = TRUE;
            else                      DI[ndi].isdir = FALSE;
            DI[ndi].size = st.st_size;
            DI[ndi].mtime = st.st_mtime;
            ndi++;
        }

    skip_it:
        free (p);
    }
    closedir (df);
    free (p1);

    dmsg ("returning from match2()\n");
    return 0;
}

/* ------------------------------------------------------------- */

static void drawline_del_down (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[2048];
    int  length, ln;
    struct tm *tmp;

    ln = strlen (sync_l_root);
    tmp = localtime (&DI[n].mtime);

    snprintf1 (buf, sizeof(buf), "%11s %02d/%02d/%04d %s",
               DI[n].isdir ? M("<DIR>") : pretty64(DI[n].size),
               tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
               DI[n].path+ln+1);

    length = strlen (buf);

    video_put_n_char (' ', len, row, col);
    if (length > shift)
        video_put_str (buf+shift, min1(len, length-shift), row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}

/* ------------------------------------------------------------- */

static void drawline_del_up (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[2048];
    int  length, ln;
    struct tm *tmp;

    ln = strlen (sync_r_root);
    tmp = localtime (&DI[n].mtime);

    snprintf1 (buf, sizeof(buf), "%11s %02d/%02d/%04d %s",
               DI[n].isdir ? M("<DIR>") : pretty64(DI[n].size),
               tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
               DI[n].path+ln+1);

    length = strlen (buf);

    video_put_n_char (' ', len, row, col);
    if (length > shift)
        video_put_str (buf+shift, min1(len, length-shift), row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}

/* ------------------------------------------------------------- */

static void drawline_transfer_down (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[2048];
    int  length, ln;
    struct tm *tmp;

    ln = strlen (sync_r_root);
    tmp = localtime (&Tm[n].mtime);

    if (strlen (Tm[n].r_dir) == ln)
    {
        snprintf1 (buf, sizeof(buf), "%11s %02d/%02d/%04d %s",
                   Tm[n].flags & TF_DIRECTORY ? M("<DIR>") : pretty64(Tm[n].size),
                   tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
                   Tm[n].r_name);
    }
    else
    {
        snprintf1 (buf, sizeof(buf), "%11s %02d/%02d/%04d %s/%s",
                   Tm[n].flags & TF_DIRECTORY ? M("<DIR>") : pretty64(Tm[n].size),
                   tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
                   Tm[n].r_dir+ln+1, Tm[n].r_name);
    }
    length = strlen (buf);

    video_put_n_char (' ', len, row, col);
    if (length > shift)
        video_put_str (buf+shift, min1(len, length-shift), row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}

/* ------------------------------------------------------------- */

static void drawline_transfer_up (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[2048];
    int  length, ln;
    struct tm *tmp;

    ln = strlen (sync_l_root);
    tmp = localtime (&Tm[n].mtime);

    if (strlen (Tm[n].l_dir) == ln)
    {
        snprintf1 (buf, sizeof(buf), "%11s %02d/%02d/%04d %s",
                   Tm[n].flags & TF_DIRECTORY ? M("<DIR>") : pretty64(Tm[n].size),
                   tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
                   Tm[n].l_name);
    }
    else
    {
        snprintf1 (buf, sizeof(buf), "%11s %02d/%02d/%04d %s/%s",
                   Tm[n].flags & TF_DIRECTORY ? M("<DIR>") : pretty64(Tm[n].size),
                   tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
                   Tm[n].l_dir+ln+1, Tm[n].l_name);
    }

    length = strlen (buf);

    video_put_n_char (' ', len, row, col);
    if (length > shift)
        video_put_str (buf+shift, min1(len, length-shift), row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}

/* ------------------------------------------------------------------ */

static int display_transfer_list (int direction)
{
    if (direction == DOWN)
    {
        fly_choose (M("Remote files to be downloaded "
                      "(no real transfer will be performed)"),
                    CHOOSE_ALLOW_HSCROLL, &ntm, 0,
                    drawline_transfer_down, NULL);
    }
    else
    {
        fly_choose (M("Local files to be uploaded "
                      "(no real transfer will be performed)"),
                    CHOOSE_ALLOW_HSCROLL, &ntm, 0,
                    drawline_transfer_up, NULL);
    }

    return 0;
}

/* ------------------------------------------------------------------ */

static int display_delete_list (int direction)
{
    if (direction == DOWN)
    {
        fly_choose (M("Local files to be deleted (deleting will NOT be performed)"),
                    CHOOSE_ALLOW_HSCROLL, &ndi, 0,
                    drawline_del_down, NULL);
    }
    else
    {
        fly_choose (M("Remote files to be deleted (deleting will NOT be performed)"),
                    CHOOSE_ALLOW_HSCROLL, &ndi, 0,
                    drawline_del_up, NULL);
    }

    return 0;
}

/* ------------------------------------------------------------- */

static void compile_re (mirror_options_t *mo)
{
    int nw, i;
    char *p, **w;

    mo->d_fnm_valid = FALSE;
    if (mo->ex_dir_glob != NULL && mo->ex_dir_glob[0] != '\0')
    {
        p = strdup (mo->ex_dir_glob);
        nw = str_split (p, &w, ',');
        mo->nd_fnm = nw;
        mo->d_fnm = xmalloc (sizeof(char *) * nw);
        for (i=0; i<nw; i++)
        {
            mo->d_fnm[i] = strdup (w[i]);
        }
        free (p);
        free (w);
        mo->d_fnm_valid = TRUE;
    }

    mo->f_fnm_valid = FALSE;
    if (mo->ex_file_glob != NULL && mo->ex_file_glob[0] != '\0')
    {
        p = strdup (mo->ex_file_glob);
        nw = str_split (p, &w, ',');
        mo->nf_fnm = nw;
        mo->f_fnm = xmalloc (sizeof(char *) * nw);
        for (i=0; i<nw; i++)
        {
            mo->f_fnm[i] = strdup (w[i]);
        }
        free (p);
        free (w);
        mo->f_fnm_valid = TRUE;
    }

    mo->d_re_valid = FALSE;
    if (mo->ex_dir_regex != NULL && mo->ex_dir_regex[0] != '\0')
    {
        if (regcomp (&mo->d_re, mo->ex_dir_regex, REG_EXTENDED|REG_NOSUB|
                     (mo->case_insensitive ? REG_ICASE : 0)) == 0)
        {
            mo->d_re_valid = TRUE;
        }
        else
        {
            fly_ask_ok (0, M("Regular expression '%s' is invalid "
                             "and will be ignored"),
                        mo->ex_dir_regex);
        }
    }

    mo->f_re_valid = FALSE;
    if (mo->ex_file_regex != NULL && mo->ex_file_regex[0] != '\0')
    {
        if (regcomp (&mo->f_re, mo->ex_file_regex, REG_EXTENDED|REG_NOSUB|
                     (mo->case_insensitive ? REG_ICASE : 0)) == 0)
        {
            mo->f_re_valid = TRUE;
        }
        else
        {
            fly_ask_ok (0, M("Regular expression '%s' is invalid "
                             "and will be ignored"),
                        mo->ex_file_regex);
        }
    }

    if (mo->f_fnm_valid)
    {
        dmsg ("file glob: %d items:\n", mo->nf_fnm);
        for (i=0; i<mo->nf_fnm; i++)
            dmsg (" [%s]\n", mo->f_fnm[i]);
    }

    if (mo->d_fnm_valid)
    {
        dmsg ("dir glob: %d items:\n", mo->nd_fnm);
        for (i=0; i<mo->nd_fnm; i++)
            dmsg (" [%s]\n", mo->d_fnm[i]);
    }
}

/* ------------------------------------------------------------- */

#define fetch_bool(sect,name,var) \
    if (cfg_check_boolean (CONFIG_NFTP, sect, name)) \
        var = cfg_get_boolean (CONFIG_NFTP, sect, name);

#define fetch_int(sect,name,var) \
    if (cfg_check_integer (CONFIG_NFTP, sect, name)) \
        var = cfg_get_integer (CONFIG_NFTP, sect, name);

#define fetch_str(sect,name,var) \
    if (cfg_check_string (CONFIG_NFTP, sect, name)) \
    { \
       p = cfg_get_string (CONFIG_NFTP, sect, name); \
       if (p != NULL && p[0] != '\0') var = strdup (p); \
    }

int get_mirror_options (mirror_options_t *mo)
{
    char *p;

    /* first we set defaults */
    mo->delete_unmatched = FALSE;
    mo->recurse_subdirs = TRUE;
    mo->ignore_timestamps = FALSE;
    mo->confirm_deletions = TRUE;
    mo->dry_run = FALSE;
    mo->time_jitter = 0;
    mo->ex_dir_glob = strdup ("");
    mo->ex_dir_regex = strdup ("");
    mo->ex_file_glob = strdup ("");
    mo->ex_file_regex = strdup ("");

    /* retrieve values from nftp.cfg */
    fetch_bool ("mirror", "delete-unmatched", mo->delete_unmatched);
    fetch_bool ("mirror", "recurse-subdirs", mo->recurse_subdirs);
    fetch_bool ("mirror", "ignore-timestamps", mo->ignore_timestamps);
    fetch_bool ("mirror", "confirm-deletions", mo->confirm_deletions);
    fetch_bool ("mirror", "dry-run", mo->dry_run);
    fetch_bool ("mirror", "case-insensitive", mo->case_insensitive);

    fetch_int  ("mirror", "time-jitter", mo->time_jitter);

    fetch_str ("mirror", "exclude-dir-glob",  mo->ex_dir_glob);
    fetch_str ("mirror", "exclude-dir-regex", mo->ex_dir_regex);
    fetch_str ("mirror", "exclude-file-glob",  mo->ex_file_glob);
    fetch_str ("mirror", "exclude-file-regex", mo->ex_file_regex);

    compile_re (mo);

    return 0;
}

/* ------------------------------------------------------------- */

int set_mirror_options (mirror_options_t *mo)
{
    /* put values into nftp.cfg */
    cfg_set_boolean (CONFIG_NFTP, "mirror", "delete-unmatched", mo->delete_unmatched);
    cfg_set_boolean (CONFIG_NFTP, "mirror", "recurse-subdirs", mo->recurse_subdirs);
    cfg_set_boolean (CONFIG_NFTP, "mirror", "ignore-timestamps", mo->ignore_timestamps);
    cfg_set_boolean (CONFIG_NFTP, "mirror", "confirm-deletions", mo->confirm_deletions);
    cfg_set_boolean (CONFIG_NFTP, "mirror", "dry-run", mo->dry_run);
    cfg_set_boolean (CONFIG_NFTP, "mirror", "case-insensitive", mo->case_insensitive);

    cfg_set_integer (CONFIG_NFTP, "mirror", "time-jitter", mo->time_jitter);

    cfg_set_string (CONFIG_NFTP, "mirror", "exclude-dir-glob",  mo->ex_dir_glob);
    cfg_set_string (CONFIG_NFTP, "mirror", "exclude-dir-regex", mo->ex_dir_regex);
    cfg_set_string (CONFIG_NFTP, "mirror", "exclude-file-glob",  mo->ex_file_glob);
    cfg_set_string (CONFIG_NFTP, "mirror", "exclude-file-regex", mo->ex_file_regex);

    write_config ();
    return 0;
}

/* ------------------------------------------------------------- */

int edit_mirror_options (mirror_options_t *mo)
{
    int        reply;
    entryfield_t mopt[11];

    mopt[0].banner = M("Delete files not present in the source?");
    mopt[0].type = EF_BOOLEAN;
    mopt[0].value.boolean = mo->delete_unmatched;
    mopt[0].flags = 0;
        
    mopt[1].banner = M("Recurse into subdirectories?");
    mopt[1].type = EF_BOOLEAN;
    mopt[1].value.boolean = mo->recurse_subdirs;
    mopt[1].flags = 0;
        
    mopt[2].banner = M("Ignore file modification times?");
    mopt[2].type = EF_BOOLEAN;
    mopt[2].value.boolean = mo->ignore_timestamps;
    mopt[2].flags = 0;
        
    mopt[3].banner = M("Confirm file deletions?");
    mopt[3].type = EF_BOOLEAN;
    mopt[3].value.boolean = mo->confirm_deletions;
    mopt[3].flags = 0;
        
    mopt[4].banner = M("Exclude directories (comma-separated masks)");
    mopt[4].type = EF_STRING;
    str_scopy (mopt[4].value.string, mo->ex_dir_glob);
    mopt[4].flags = 0;
        
    mopt[5].banner = M("Exclude directories (regular expression)");
    mopt[5].type = EF_STRING;
    str_scopy (mopt[5].value.string, mo->ex_dir_regex);
    mopt[5].flags = 0;
        
    mopt[6].banner = M("Exclude files (comma-separated masks)");
    mopt[6].type = EF_STRING;
    str_scopy (mopt[6].value.string, mo->ex_file_glob);
    mopt[6].flags = 0;
        
    mopt[7].banner = M("Exclude files (regular expression)");
    mopt[7].type = EF_STRING;
    str_scopy (mopt[7].value.string, mo->ex_file_regex);
    mopt[7].flags = 0;
        
    mopt[8].banner = M("Allowed timestamp mismatch, seconds");
    mopt[8].type = EF_INTEGER;
    mopt[8].value.integer = mo->time_jitter;
    mopt[8].flags = 0;
        
    mopt[9].banner = M("Don't do anything, just check");
    mopt[9].type = EF_BOOLEAN;
    mopt[9].value.boolean = mo->dry_run;
    mopt[9].flags = 0;
        
    mopt[10].banner = M("Case insensitive filename comparisons");
    mopt[10].type = EF_BOOLEAN;
    mopt[10].value.boolean = mo->case_insensitive;
    mopt[10].flags = 0;
        
    reply = mle2 (11, mopt, M("Mirroring options"), 0);

    if (reply == PRESSED_ENTER)
    {
        mo->delete_unmatched = mopt[0].value.boolean;
        mo->recurse_subdirs = mopt[1].value.boolean;
        mo->ignore_timestamps = mopt[2].value.boolean;
        mo->confirm_deletions = mopt[3].value.boolean;
        mo->time_jitter = mopt[8].value.integer;
        mo->dry_run = mopt[9].value.boolean;
        mo->case_insensitive = mopt[10].value.boolean;

        free (mo->ex_dir_glob);
        free (mo->ex_dir_regex);
        free (mo->ex_file_glob);
        free (mo->ex_file_regex);

        mo->ex_dir_glob   = strdup (mopt[4].value.string);
        mo->ex_dir_regex  = strdup (mopt[5].value.string);
        mo->ex_file_glob  = strdup (mopt[6].value.string);
        mo->ex_file_regex = strdup (mopt[7].value.string);

        compile_re (mo);
    }

    if (reply == PRESSED_ENTER) return 0;
    return -1;
}
