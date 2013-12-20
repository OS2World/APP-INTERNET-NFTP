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

static int match1 (int D, int F, int act);
static int match2 (char *D, int act);
static void drawline_del (int n, int len, int shift, int pointer, int row, int col);

static char         *sync_l_root, *sync_r_root;
static trans_item   *Tm;
static int          ntm, ntma;

/* list of files/directories to delete. these are absolute and
 include sync_*_root */
static del_item_t   *DI;
static int          ndi, ndia, tot_files, tot_dirs;

/* ------------------------------------------------------------- */

int mirror (int d, int noisy)
{
    int  prev_status, prev_mode, prev_backups, i, rc, delfile, deldir;
    int64_t delsize;
    char *p;

    dmsg ("entered mirror(%d)\n", d);
    sync_r_root = strdup (RCURDIR.name);
    sync_l_root = strdup (local.dir.name);
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
    if (status.sync_subdirs)
    {
        for (i=0; i<RNFILES; i++)
        {
            if (RFILE(i).flags & FILE_FLAG_DIR
                && strcmp (RFILE(i).name, ".")
                && strcmp (RFILE(i).name, ".."))
            {
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
            match1 (site.cdir, i, ACTION_TRANSFER);
        }

        if (ntm != 0)
        {
            dmsg ("doing mirroring transfer\n");
            rc = transfer (DOWN, Tm, ntm, noisy);
            dmsg ("done mirroring transfer; rc=%d\n", rc);
            if (rc) return rc;
        }

        /* remove unmatched files if needed */
        if (status.sync_delete)
        {
            //put_message (MSG(M_DEL_UNMATCHED));
            put_message ("  Collecting list of files/directories to delete...  ");
            dmsg ("doing delete\n");
            match2 (sync_l_root, ACTION_DELETE);

            if (ndi != 0)
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
                if (noisy)
                {
                    fly_ask_ok (ASK_WARN,
                                " %d files, %d directories, total size %s bytes \n"
                                " are going to be DELETED from LOCAL DISK. \n"
                                " Please review list of files and confirm deletion. \n"
                                " Press Enter to see list of files ",
                                delfile, deldir, pretty64(delsize));
                    /* we ask user when running in interactive mode */
                    rc = fly_choose (" Press Enter to delete unmatched files, Esc to cancel ",
                                     CHOOSE_ALLOW_HSCROLL, &ndi, 0,
                                     drawline_del, NULL);
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

        /* issue message if nothing has to be done */
        if (ntm == 0 && ndi == 0)
        {
            PutLineIntoResp2 (MSG(M_SYNC3));
            if (noisy)
                fly_ask_ok (0, MSG(M_SYNC3));
        }
        change_dir (sync_r_root);
    }

    else if (d == REMOTE_WITH_LOCAL)
    {
        put_message ("  Scanning local directory...  ");
        match2 (sync_l_root, ACTION_TRANSFER);
        if (ntm != 0)
        {
            rc = transfer (UP, Tm, ntm, noisy);
            if (rc) return rc;
        }

        // remove unmatched files if needed
        if (status.sync_delete)
        {
            //put_message (MSG(M_DEL_UNMATCHED));
            put_message ("  Collecting list of remote files/directories to delete...  ");
            for (i=0; i<site.dir[site.cdir].nfiles; i++)
            {
                if (strcmp (site.dir[site.cdir].files[i].name, "..") == 0) continue;
                match1 (site.cdir, i, ACTION_DELETE);
            }

            dmsg ("%d Files/directories to delete:\n", ndi);
            for (i=0; i<ndi; i++)
                dmsg ("%d. %s (%s) %s\n", i+1, DI[i].path,
                      DI[i].isdir ? "dir" : "file", pretty64(DI[i].size));

            if (ndi != 0)
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
                if (noisy)
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
                                     drawline_del, NULL);
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

        change_dir (sync_r_root);

        if (ntm == 0 && ndi == 0)
        {
            PutLineIntoResp2 (MSG(M_SYNC3));
            if (noisy)
                fly_ask_ok (0, MSG(M_SYNC3));
        }

        retrieve_dir (RCURDIR.name);
    }

    build_local_filelist (NULL);
    
    // switch symlink resolving back to original setting
    status.resolve_symlinks = prev_status;
    adjust_menu_status ();
    site.batch_mode = prev_mode;
    options.backups = prev_backups;
    
    /* invalidate directories on server altered by transfer/delete */
    if (d == REMOTE_WITH_LOCAL)
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

/* compare remote directories/files with local (downloading or deleting
 remote files) */
static int match1 (int D, int F, int act)
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
        if (!status.sync_subdirs) return 0;
        if (strcmp (site.dir[D].files[F].name, "..") == 0 ||
            strcmp (site.dir[D].files[F].name, ".") == 0) return 0;
        tot_dirs++;
        dmsg ("counting dir %s/%s\n", site.dir[D].name, site.dir[D].files[F].name);
        // make its name
        p = str_strdup1 (site.dir[D].name, strlen(site.dir[D].files[F].name)+2);
        str_cats (p, site.dir[D].files[F].name);
        // find it in the cache
        for (d=0; d<site.ndir; d++)
        {
            if (options.relaxed_mirror)
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
            match1 (d, i, act);
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
    if (site.dir[D].files[F].flags & FILE_FLAG_LINK)
    {
    }

    // remote is a regular file?
    if (site.dir[D].files[F].flags & FILE_FLAG_REGULAR)
    {
        // first we determine the subdirectory
        if (str_headcmp (site.dir[D].name, sync_r_root) != 0) return 0;
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
            dmsg ("local mtime = %u, remote mtime = %u\n", st.st_mtime, site.dir[D].files[F].mtime);
            if (st.st_size != site.dir[D].files[F].size) goto get_it;
            if (is_OS2)
            {
                if (abs(st.st_mtime - site.dir[D].files[F].mtime) > 1) goto get_it;
            }
            else
            {
                /* get remote file if remote file is newer than local */
                if (st.st_mtime < site.dir[D].files[F].mtime) goto get_it;
            }
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
static int match2 (char *D, int act)
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
        if (options.relaxed_mirror)
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

        if (!status.sync_subdirs && S_ISDIR(st.st_mode)) goto skip_it;

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
                if (options.relaxed_mirror)
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
            // directory
            match2 (p, act);
            if (rf == -1) goto put_it;
        }
        else if (S_ISLNK (st.st_mode))
        {
            // symlinks are ignored for now
            goto skip_it;
        }
        else if (S_ISREG (st.st_mode))
        {
            // regular files
            if (rf == -1) goto put_it;

            /* do not cause deletings if remote and local files differ! */
            if (act == ACTION_DELETE) goto skip_it;

            /* upload when local file is newer than remote */
            if (st.st_mtime > site.dir[rd].files[rf].mtime) goto put_it;
            if (st.st_size != site.dir[rd].files[rf].size) goto put_it;
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

static void drawline_del (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[2048];
    int  length;

    snprintf1 (buf, sizeof(buf), "%11s %s",
               DI[n].isdir ? "<DIR>" : pretty64(DI[n].size), DI[n].path);
    length = strlen (buf);

    video_put_n_char (' ', len, row, col);
    if (length > shift)
        video_put_str (buf+shift, length-shift, row, col);
    video_put_n_attr (pointer ? fl_clr.selbox_pointer : fl_clr.selbox_back, len, row, col);
}
