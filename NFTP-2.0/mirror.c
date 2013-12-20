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

static int match1 (int nsite, int D, int F, int act);
static int match2 (int nsite, char *D, int act);

static char         *sync_l_root, *sync_r_root;
static trans_item   *Tm;
static int          ntm, ntma;

/* ------------------------------------------------------------- */

int mirror (int nsite, int nloc, int d, int noisy)
{
    int            prev_status, prev_mode, i, j;

    dmsg ("[john] entered mirror(%d)\n", d);
    sync_r_root = strdup (RN_CURDIR.name);
    sync_l_root = strdup (local[nloc].dir.name);
    ntma = 16;
    ntm = 0;
    Tm = malloc (sizeof (Tm[0]) * ntma);

    // switch symlink resolving off
    prev_status = status.resolve_symlinks[nsite];
    prev_mode = status.batch_mode;
    status.batch_mode = TRUE;
    status.resolve_symlinks[nsite] = FALSE;

    // first we need to scan remote tree -- if needed, of course
    if (status.sync_subdirs)
    {
        for (i=0; i<RN_NF; i++)
        {
            if (RN_FILE(i).flags & FILE_FLAG_DIR
                && strcmp (RN_FILE(i).name, ".."))
                traverse_directory (nsite, RN_FILE(i).name, FALSE);
        }
    }

    if (d == LOCAL_WITH_REMOTE)
    {
        // now we walk the tree and compile the transfer list
        for (i=0; i<site[nsite].dir[site[nsite].cdir].nfiles; i++)
        {
            if (strcmp (site[nsite].dir[site[nsite].cdir].files[i].name, "..") == 0) continue;
            match1 (nsite, site[nsite].cdir, i, ACTION_TRANSFER);
        }
        if (ntm != 0)
        {
            dmsg ("[john] doing transfer\n");
            transfer (DOWN, nsite, -1, Tm, ntm, noisy);
            dmsg ("[john] done transfer\n");
        }
        else
        {
            PutLineIntoResp (RT_COMM,nsite, MSG(M_SYNC3));
            if (noisy)
                fly_ask_ok (0, MSG(M_SYNC3));
        }
        // remove unmatched files if needed
        if (status.sync_delete)
        {
            put_message (MSG(M_DEL_UNMATCHED));
            dmsg ("[john] doing delete\n");
            match2 (nsite, sync_l_root, ACTION_DELETE);
            dmsg ("[john] done delete\n");
        }
    }

    if (d == REMOTE_WITH_LOCAL)
    {
        match2 (nsite, sync_l_root, ACTION_TRANSFER);
        if (ntm != 0)
        {
            transfer (UP, nsite, -1, Tm, ntm, noisy);
        }
        else
        {
            PutLineIntoResp (RT_COMM,nsite, MSG(M_SYNC3));
            if (noisy)
                fly_ask_ok (0, MSG(M_SYNC3));
        }

        // remove unmatched files if needed
        if (status.sync_delete)
        {
            put_message (MSG(M_DEL_UNMATCHED));
            for (i=0; i<site[nsite].dir[site[nsite].cdir].nfiles; i++)
            {
                if (strcmp (site[nsite].dir[site[nsite].cdir].files[i].name, "..") == 0) continue;
                match1 (nsite, site[nsite].cdir, i, ACTION_DELETE);
            }
        }
        // kill cached version of just updated directories
        dmsg ("before cleanup: %d dirs\n", site[nsite].ndir);
        dump_dirs (nsite);
    again:
        for (i=0; i<site[nsite].ndir; i++)
        {
            if (str_headcmp (site[nsite].dir[i].name, sync_r_root) == 0)
            {
                dmsg ("freeing %s\n", site[nsite].dir[i].name);
                free (site[nsite].dir[i].files);
                for (j=i; j<site[nsite].ndir-1; j++)
                    site[nsite].dir[j] = site[nsite].dir[j+1];
                if (site[nsite].ndir > 1) site[nsite].ndir--;
                goto again;
            }
        }
        dump_dirs (nsite);
        dmsg ("after cleanup: %d dirs\n", site[nsite].ndir);
        site[nsite].cdir = 0;
        r_chdir (nsite, sync_r_root, FALSE);
        //retrieve_dir (sync_r_root, -1);
    }

    if (d == LOCAL_WITH_REMOTE)
    {
        l_chdir (V_LEFT, NULL);
        l_chdir (V_RIGHT, NULL);
    }

    // switch symlink resolving back to original setting
    status.resolve_symlinks[nsite] = prev_status;
    status.batch_mode = prev_mode;

    free (sync_r_root);
    free (sync_l_root);
    free (Tm);
    dmsg ("[john] returning from mirror()\n");
    return 0;
}

/* ------------------------------------------------------------- */

static int match1 (int nsite, int D, int F, int act)
{
    char          *p, *p1;
    int           d, i, rc;
    struct stat   st;

    // see if remote is a directory. if it is, change to it and
    // walk it
    dmsg ("[john] entering match1(%d): [%s] in [%s]; size=%lu, mtime=%lu\n",
          act, site[nsite].dir[D].name, site[nsite].dir[D].files[F].name, site[nsite].dir[D].files[F].size, site[nsite].dir[D].files[F].mtime);
    if (site[nsite].dir[D].files[F].flags & FILE_FLAG_DIR &&
        strcmp(site[nsite].dir[D].files[F].name, ".."))
    {
        // ignore dirs if 'subdirs' mode is not set
        if (!status.sync_subdirs) return 0;
        // make its name
        p = str_strdup1 (site[nsite].dir[D].name, strlen(site[nsite].dir[D].files[F].name)+2);
        str_cats (p, site[nsite].dir[D].files[F].name);
        // find it in the cache
        for (d=0; d<site[nsite].ndir; d++)
        {
            if (options.relaxed_mirror)
            {
                if (stricmp1 (p, site[nsite].dir[d].name) == 0) break;
            }
            else
            {
                if (strcmp (p, site[nsite].dir[d].name) == 0) break;
            }
        }
        free (p);
        if (d == site[nsite].ndir) return 0; // not cached; ignore it
        // walk it
        for (i=0; i<site[nsite].dir[d].nfiles; i++)
        {
            match1 (nsite, d, i, act);
        }
        if (act == ACTION_DELETE)
        {
            // make local path corresponding to that remote directory
            if (str_headcmp (site[nsite].dir[D].name, sync_r_root) != 0) return 0;
            p = site[nsite].dir[D].name + strlen (sync_r_root);
            p1 = str_strdup1 (sync_l_root, strlen(p)+2+strlen(site[nsite].dir[D].files[F].name));
            str_cats (p1, p);
            str_cats (p1, site[nsite].dir[D].files[F].name);
            if (access (p1, F_OK) < 0)
            {
                set_wd (nsite, site[nsite].dir[D].name, FALSE);
                SendToServer (nsite, TRUE, NULL, NULL, "RMD %s", site[nsite].dir[D].files[F].name);
            }
            free (p1);
        }
    }

    // remote is a symbolic link?
    if (site[nsite].dir[D].files[F].flags & FILE_FLAG_LINK)
    {
    }

    // remote is a regular file?
    if (site[nsite].dir[D].files[F].flags & FILE_FLAG_REGULAR)
    {
        // first we determine the subdirectory
        if (str_headcmp (site[nsite].dir[D].name, sync_r_root) != 0) return 0;
        p = site[nsite].dir[D].name + strlen (sync_r_root);
        p1 = str_strdup1 (sync_l_root, strlen(p)+2+strlen(site[nsite].dir[D].files[F].name));
        str_cats (p1, p);
        str_cats (p1, site[nsite].dir[D].files[F].name);
        // p1 now holds corresponding local path
        dmsg ("[john] regular file with local pathname [%s]\n", p1);

        if (act == ACTION_TRANSFER)
        {
            rc = stat (p1, &st);
            free (p1);
            dmsg ("[john] stat(): rc = %d, size=%s, mtime=%lu, gm2local(mtime)=%lu\n", rc,
                 printf64(st.st_size), st.st_mtime, gm2local (st.st_mtime));
            if (rc < 0) goto get_it;
            if (st.st_size != site[nsite].dir[D].files[F].size) goto get_it;
            if (gm2local (st.st_mtime) != site[nsite].dir[D].files[F].mtime) goto get_it;
            dmsg ("[john] 1. exiting match1\n");
            return 0;

        get_it:
            dmsg ("[john] need to get %s/%s as %s\n", site[nsite].dir[D].name, site[nsite].dir[D].files[F].name, p1);

            if (ntm == ntma-1)
            {
                ntma *= 2;
                Tm = realloc (Tm, sizeof (Tm[0]) * ntma);
            }

            Tm[ntm].r_dir  = strdup (site[nsite].dir[D].name);
            Tm[ntm].r_name = strdup (site[nsite].dir[D].files[F].name);
            Tm[ntm].l_dir  = str_sjoin (sync_l_root, p);
            Tm[ntm].l_name = strdup (site[nsite].dir[D].files[F].name);
            Tm[ntm].mtime  = site[nsite].dir[D].files[F].mtime;
            Tm[ntm].size   = site[nsite].dir[D].files[F].size;
            Tm[ntm].perm   = site[nsite].dir[D].files[F].rights;
            Tm[ntm].description = site[nsite].dir[D].files[F].desc;
            Tm[ntm].f = NULL;
            Tm[ntm].flags = 0;
            ntm++;
        }
        else
        {
            if (access (p1, F_OK) < 0)
            {
                dmsg ("[john] deleting remote: [%s] in [%s]\n", site[nsite].dir[D].files[F].name, site[nsite].dir[D].name);
                set_wd (nsite, site[nsite].dir[D].name, FALSE);
                SendToServer (nsite, TRUE, NULL, NULL, "DELE %s", site[nsite].dir[D].files[F].name);
            }
        }
    }

    dmsg ("[john] 2. exiting match1\n");
    return 0;
}

/* ------------------------------------------------------------- */

static int match2 (int nsite, char *D, int act)
{
    DIR            *df;
    struct dirent  *e;
    struct stat    st;
    char           *p, *p1, *p2;
    int            i, rd, rf;

    dmsg ("[john] entering match2(%d): [%s]\n", act, D);
    // see if corresponding remote directory exists
    if (str_headcmp (D, sync_l_root) != 0) return 0;
    p = D + strlen (sync_l_root);
    p1 = str_strdup1 (sync_r_root, strlen(p)+2);
    str_cats (p1, p);
    str_strip (p1, "/");
    dmsg ("[john] looking up [%s] on remote\n", p1);
    for (i=0; i<site[nsite].ndir; i++)
    {
        if (options.relaxed_mirror)
        {
            if (stricmp1 (site[nsite].dir[i].name, p1) == 0) break;
        }
        else
        {
            if (strcmp (site[nsite].dir[i].name, p1) == 0) break;
        }
    }
    if (i == site[nsite].ndir) rd = -1;
    else                rd = i;
    dmsg ("[john] found: %d\n", rd);

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

        dmsg ("[john] trying to match %s, size=%lu, mtime=%lu, gm2local(mtime)=%lu\n",
              e->d_name, st.st_size, st.st_mtime, gm2local (st.st_mtime));
        // try to locate remote equivalent if possible
        rf = -1;
        if (rd >= 0)
        {
            for (i=0; i<site[nsite].dir[rd].nfiles; i++)
            {
                if (options.relaxed_mirror)
                {
                    if (stricmp1 (site[nsite].dir[rd].files[i].name, e->d_name) == 0) break;
                }
                else
                {
                    if (strcmp (site[nsite].dir[rd].files[i].name, e->d_name) == 0) break;
                }
            }
            if (i != site[nsite].dir[rd].nfiles) rf = i;
        }
        if (rf == -1)
            dmsg ("[john] not found\n");
        else
            dmsg ("[john] located: [%s], size=%lu, mtime=%lu\n",
                  site[nsite].dir[rd].files[rf].name, site[nsite].dir[rd].files[rf].size, site[nsite].dir[rd].files[rf].mtime);

        // by type
        if (S_ISDIR (st.st_mode))
        {
            // directory
            match2 (nsite, p, act);
        }
        else if (S_ISLNK (st.st_mode))
        {
            // symlinks are ignored for now
            goto skip_it;
        }
        else if (S_ISREG (st.st_mode))
        {
            if (rf == -1) goto put_it;
            if (gm2local (st.st_mtime) > site[nsite].dir[rd].files[rf].mtime) goto put_it;
            if (st.st_size != site[nsite].dir[rd].files[rf].size) goto put_it;
        }
        goto skip_it;

    put_it:
        if (act == ACTION_TRANSFER)
        {
            dmsg ("need to put %s as %s/%s\n", p, p1, e->d_name);
            if (ntm == ntma-1)
            {
                ntma *= 2;
                Tm = realloc (Tm, sizeof (Tm[0]) * ntma);
            }
            Tm[ntm].r_dir  = strdup (p1);
            Tm[ntm].r_name = strdup (e->d_name);
            Tm[ntm].l_dir  = strdup (D);
            Tm[ntm].l_name = strdup (Tm[ntm].r_name);
            Tm[ntm].mtime  = gm2local (max1 (0, st.st_mtime));;
            Tm[ntm].size   = st.st_size;
            Tm[ntm].perm   = st.st_mode;
            Tm[ntm].description = NULL;
            Tm[ntm].f = NULL;
            Tm[ntm].flags = 0;
            ntm++;
        }
        else
        {
            p2 = str_strdup1 (D, strlen(e->d_name)+1);
            str_cats (p2, e->d_name);
            dmsg ("[john] deleting [%s]\n", p2);
            unlink (p2);
            free (p2);
        }

    skip_it:
        free (p);
    }
    closedir (df);
    free (p1);

    dmsg ("[john] returning from match2()\n");
    return 0;
}
