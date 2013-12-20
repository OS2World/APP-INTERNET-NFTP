/*
 Model for file times processed by NFTP
 --------------------------------------
 time() and stat() return UTC. utimes() uses UTC too.
 Remote FTP servers return local time.
 We process everything in UTC.
 Except one thing, namely displaying times to user.
 */
#include <fly/fly.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#ifdef __MINGW32__
   #include <sys/utime.h>
   #include <io.h>
   #define ftruncate chsize
#else
   #include <utime.h>
#endif

#ifdef __MINGW32__
   #define wait_a_bit(a) sleep1 (1.0)
#else
   #define wait_a_bit(a) tv.tv_sec = 0; tv.tv_usec = a; \
                         select (0, NULL, NULL, NULL, &tv)
#endif


#include "nftp.h"
#include "net.h"
#include "local.h"
#include "history.h"
#include "auxprogs.h"
#include "network.h"
#include "keys.h"
#include "befs.h"

enum
{
    TR_OK        = 0, // transfer went OK
    TR_RETRY     = 1, // need to retry
    TR_SKIP      = 2, // user action: skip this file
    TR_CANCEL    = 3  // user action: cancel entire transfer
};

enum
{
    ACT_OVERWRITE = 1,
    ACT_REGET     = 2,
    ACT_SKIP      = 3,
    ACT_CANCEL    = 4,
    ACT_RENAME    = 5
};

/* ------------------------------------------------------------- */

typedef struct
{
    int      files;
    int64_t  bytes;
}
progress_data;

// to -- total for entire transfer; tr -- sent via network interface,
// sk -- skipped due to reget
typedef struct
{
    progress_data to, tr, sk; /* re = to - tr - sk */
    double        spent; /* start, total, paused */
}
progress;

static int      http_authenticate = FALSE, port_failed = 0;

/* ------------------------------------------------------------- */
int create_dir (char *d);
static RC IssuePASVCommand (unsigned long *ip, int *port);
static void ShowProgress (char *rfilename, char *lfilename, int ind_type,
                          int where, double stall, progress *tot,
                          progress *cur, double *interval_start);
static int swallow_http_headers (int sock);
static int transfer_data (int where, int sock, int hnd, char *LF, char *RF,
                          int ind_type, progress *tot, progress *cur,
                          double *interval_start);
static int transfer_restart (int where, trans_item T, char *LF,
                             int64_t *restsize, int nt);
static int open_handle (int where, int action, char *LF);
static int issue_transfer_cmd (int where, int action, char *cmd, int64_t lsize, int *sock);
static void transfer_bfs_attrs (int where, trans_item *T);
static char *compose_command (int where, trans_item T, int action);
static void set_properties (int where, trans_item T, char *LF);
static int get_mtime (trans_item *T);

static void dump_progress (progress tot, progress cur)
{
    dmsg (" tot: to: files = %d, bytes = %qd\n"
          "      tr: files = %d, bytes = %qd\n"
          "      sk: files = %d, bytes = %qd\n"
          " cur: to: files = %d, bytes = %qd\n"
          "      tr: files = %d, bytes = %qd\n"
          "      sk: files = %d, bytes = %qd\n",
          tot.to.files, tot.to.bytes,
          tot.tr.files, tot.tr.bytes,
          tot.sk.files, tot.sk.bytes,
          cur.to.files, cur.to.bytes,
          cur.tr.files, cur.tr.bytes,
          cur.sk.files, cur.sk.bytes);
}

#define cm(a,b) ((a)==(b) ? 0 : ((a)>(b) ? 1 : -1))

/* ------------------------------------------------------------- */
static int cmp_direction;
static int cmp_trans (const void *e1, const void *e2)
{
    trans_item *T1, *T2;
    //int        c;
    char       f1[8192], f2[8192];

    T1 = (trans_item *) e1;
    T2 = (trans_item *) e2;

    if (cmp_direction == UP)
    {
        snprintf1 (f1, sizeof(f1), "%s/%s%s", T1->l_dir, T1->l_name,
                   T1->flags & TF_DIRECTORY ? "/" : "");
        snprintf1 (f2, sizeof(f2), "%s/%s%s", T2->l_dir, T2->l_name,
                   T2->flags & TF_DIRECTORY ? "/" : "");
        return strcmp (f1, f2);
    }
    else
    {
        snprintf1 (f1, sizeof(f1), "%s/%s%s", T1->r_dir, T1->r_name,
                   T1->flags & TF_DIRECTORY ? "/" : "");
        snprintf1 (f2, sizeof(f2), "%s/%s%s", T2->r_dir, T2->r_name,
                   T2->flags & TF_DIRECTORY ? "/" : "");
        return strcmp (f1, f2);
    }

#if 0
    /* directories take precedence */
    //c = cm (T1->flags & TF_DIRECTORY, T2->flags & TF_DIRECTORY);
    //if (c) return -c;

    switch (cmp_direction)
    {
    case DOWN:
    case LIST:
    case LSATTRS:
    case SETATTRS:
    case RETATTRS:
    default:
        c = strcmp (T1->r_dir, T2->r_dir);
        if (c) return c;
        c = strcmp (T1->r_name, T2->r_name);
        return c;

    case UP:
        c = strcmp (T1->l_dir, T2->l_dir);
        if (c) return c;
        c = strcmp (T1->l_name, T2->l_name);
        return c;
    }
#endif
}

/* ------------------------------------------------------------- */
RC transfer (int where, trans_item *T, int nt, int noisy)
{
    int             rc, hnd = -1, ind_type;
    char            *cmd, *RF, *LF, *p, **dirs;
    struct          stat st;
    int             sock=-1, sock1 = 0, rsp, ndirs;
    //, is_listing=FALSE;
    int             i, trc, action;
    int64_t         lsize;
    progress        tot, cur;
    double          interval_start;
    
    if (nt <= 0) return 0;

    /* sort files/directories to transfer: directories first, then
     files in alphabetical order of their full pathnames */
    cmp_direction = where;
    qsort (T, nt, sizeof(trans_item), cmp_trans);

    dmsg ("%d sorted items to transfer (where=%d):\n", nt, where);
    for (i=0; i<nt; i++)
        dmsg ("%d. rem=%s/%s, loc=%s/%s, mtime=%u, size=%s, flags=%x\n",
              i+1, T[i].r_dir, T[i].r_name, T[i].l_dir, T[i].l_name,
              T[i].mtime, pretty64(T[i].size), T[i].flags);

    /* initialize progress data */
    tot.to.files = nt;
    tot.to.bytes = 0;
    for (i=0; i<nt; i++) tot.to.bytes += T[i].size;
    tot.tr.files = 0;
    tot.tr.bytes = 0;
    tot.sk = tot.tr;
    tot.spent = 0.;
    //is_listing = T[0].r_name[0] == '\0';
    if (where == DOWN || where == UP) ind_type = PROGRESS_SCREEN;
    else                              ind_type = PROGRESS_MSG;
    port_failed = 0;

    put_message (M ("Starting transfer..."));

    /* check for symlinks in the list */
    /*
    if (direction == DOWN)
    {
        for (i=0; i<nt; i++)
        {
            if (T[i].flags & FILE_FLAG_LINK)
            {
                rc = set_wd (T[i].r_dir, FALSE);
                if (rc) continue;
                if (site.features.has_size)
                {
                    rc = SendToServer (TRUE, &rsp, buf, "SIZE %s", T[i].r_name);
                    if (rc) continue;
                    if (rsp != 2)
                    {
                        site.features.has_size = FALSE;
                        continue;
                    }
                    
                }
            }
        }
    }
    */
    
    /* do the outer cycle (by files) */
    trc = TR_OK;
    for (i=0; i<nt; i++)
    {
        hnd = -1;
        interval_start = clock1 ();
        
        sock = 0;
        sock1 = 0;
        RF = str_sjoin (T[i].r_dir, T[i].r_name);
        LF = str_sjoin (T[i].l_dir, T[i].l_name);

        cur.to.bytes = T[i].size;
        cur.tr.bytes = 0;
        cur.sk = cur.tr;
        cur.spent = 0.;
        //cur.start = clock1 ();

        /* when directory transfer is specified we only create directories */
        if (T[i].flags & TF_DIRECTORY)
        {
            if (where == DOWN)
            {
                /* create local directory */
                rc = make_subtree (LF);
                if (rc)
                {
                    PutLineIntoResp2 (M ("Cannot create directory '%s'"), LF);
                    goto skip;
                }
            }
            else if (where == UP)
            {
                /* create remote directory */
                rc = create_dir (RF);
                if (rc) goto skip;
            }
            if (T[i].f != NULL) T[i].f->flags &= ~FILE_FLAG_MARKED;
            goto further;
        }

        if (make_subtree (T[i].l_dir) < 0)
        {
            PutLineIntoResp2 (M ("Cannot create directory '%s'"), T[i].l_dir);
            goto further;
        }

        if (where == DOWN && T[i].f != NULL) get_mtime (T+i);

        // find out whether we need to restart/skip/overwrite
        action = transfer_restart (where, T[i], LF, &lsize, nt);
        if (action == ACT_CANCEL)
        {
            free (RF); free (LF);
            return ERR_CANCELLED;
        }

    restart:
        if (action == ACT_SKIP)
        {
            cur.sk.bytes += T[i].size;
            tot.sk.bytes += T[i].size;
            if (T[i].f != NULL) T[i].f->flags &= ~FILE_FLAG_MARKED;
            //set_properties (where, T[i], LF);
            goto skip;
        }

        if (action == ACT_REGET &&
            (is_http_proxy ||
             (where == DOWN && !site.features.rest_works) ||
             (where == UP && !site.features.appe_works) ||
             site.system_type == SYS_AS400 ||
             site.system_type == SYS_VMS_MULTINET
            )
           ) action = ACT_OVERWRITE;

        hnd = open_handle (where, action, LF);
        if (where == DOWN && hnd < 0/* && IS_OS2*/ && options.shorten_names)
        {
            // try shortened version of file name if possible
            p = str_shorten (T[i].l_name);
            if (p != NULL)
            {
                strcpy (LF, T[i].l_dir);
                str_translate (LF, '\\', '/');
                str_cats (LF, p);
                hnd = open_handle (where, action, LF);
            }
        }
        if (hnd < 0)
        {
            if (is_batchmode)
                PutLineIntoResp2 (M ("Cannot open file '%s' for writing"), LF);
            else
            {
                if (where == DOWN)
                    fly_ask_ok (ASK_WARN, M ("Cannot open file '%s' for writing"), LF);
                else
                    fly_ask_ok (ASK_WARN, M ("Cannot open file '%s' for reading"), LF);
            }
            goto skip;
        }
        //dmsg ("opened %d\n", hnd);

        // change directory on remote
        rc = set_wd (T[i].r_dir, FALSE);
        if (rc)
        {
            //if (where == DOWN) goto skip;
            // try to create remote directory when uploading
            if (where == UP)
            {
                rc = create_dir (T[i].r_dir);
                if (rc) goto skip;
                rc = set_wd (T[i].r_dir, FALSE);
                if (rc) goto skip;
            }
            else
                goto skip;
        }

        // compose transfer command
    retry:
    retry_auth:
        
        cmd = compose_command (where, T[i], action);
        if (cmd == NULL) goto skip;
        
        rc = issue_transfer_cmd (where, action, cmd, lsize, &sock);
        free (cmd);
        switch (rc)
        {
        case ERR_TRANSIENT:
            goto retry;
        case ERR_FATAL:
            close (hnd);
            goto skip;
        case ERR_SKIP:
            close (hnd);
            goto skip;
        case ERR_CANCELLED:
            close (hnd);
            goto cancelled;
        case ERR_AUTHENTICATE:
            goto retry_auth;
        case ERR_RESTFAIL:
            tot.tr.bytes -= cur.tr.bytes;
            cur.tr.bytes = 0;
            action = ACT_OVERWRITE;
            close (hnd);
            hnd = open_handle (where, action, LF);
            goto retry;
        }

        if (action == ACT_REGET)
        {
            lseek (hnd, lsize, SEEK_SET);
            cur.sk.bytes += lsize-cur.tr.bytes;
            tot.sk.bytes += lsize-cur.tr.bytes;
        }

        //if (ind_type == PROGRESS_SCREEN) set_view_mode (VIEW_TRANSFERSTATS);

        /*********************************************************/
        trc = transfer_data (where, sock, hnd, LF, RF, ind_type, &tot, &cur, &interval_start);
        dmsg ("transfer_data() returned %d\n", trc);
        /*********************************************************/

        if (is_http_proxy)
        {
            if (where == UP)
            {
                if (trc == TR_RETRY) trc = TR_SKIP;
            }
        }
        else
        {
            // see what server has to say
            ShowProgress (RF, LF, ind_type, where, -2.0, &tot, &cur, &interval_start);
            rc = SendToServer (FALSE, &rsp, NULL, NULL);
            if (rc) rsp = -1;
        }
        dmsg ("closing %d\n", hnd);
        close (hnd);
        hnd = -1;

        // check whether we got the entire file
        dmsg ("comparing sizes: reget=%s, trans=%s, tsize=%s\n",
              printf64(cur.sk.bytes), printf64(cur.tr.bytes), printf64(cur.to.bytes));
        if (trc == TR_OK &&
            !is_http_proxy &&
            status.binary &&
            (cur.sk.bytes + cur.tr.bytes < cur.to.bytes) &&
            (rsp != 2 || where == UP)
           )
        {
            dmsg ("now willing to retry\n");
            trc = TR_RETRY;
        }

        switch (trc)
        {
        case TR_OK:
            dmsg ("transfer: OK\n");
            if (where == DOWN || where == UP)
                log_transfers ("%8s (%6lu B/s) - %s:%s\n", printf64(T[i].size),
                               (unsigned long)site.speed, site.u.hostname, RF);
            set_properties (where, T[i], LF);
            if (T[i].f != NULL) T[i].f->flags &= ~FILE_FLAG_MARKED;
            goto further;
            
        case TR_RETRY:
            dmsg ("transfer: RETRY; site.features.rest_works=%d, appe_works=%d\n",
                  site.features.rest_works, site.features.appe_works);
            dump_progress (tot, cur);
            if ((where == DOWN && site.features.rest_works) ||
                (where == UP && site.features.appe_works))
            {
                tot.sk.bytes -= cur.sk.bytes;
                cur.sk.bytes = 0;
            }
            if ((where == DOWN && !site.features.rest_works) ||
                (where == UP && !site.features.appe_works))
            {
                tot.tr.bytes -= cur.tr.bytes;
                cur.tr.bytes = 0;
            }
            dump_progress (tot, cur);
            if (where == UP)
            {
                dmsg ("upload: re-reading directory\n");
                rc = retrieve_dir (T[i].r_dir);
                dmsg ("rc = %d\n", rc);
                if (rc) goto skip;
            }
            dmsg ("determining whether to restart transfer\n");
            action = transfer_restart (where, T[i], LF, &lsize, 999999);
            dmsg ("decision: %d\n", action);
            close (hnd);
            goto restart;
            break;
            
        case TR_SKIP:
            dmsg ("transfer: SKIP\n");
            PutLineIntoResp2 (M ("Skipped file '%s'"), RF);
            break;
            
        case TR_CANCEL:
            dmsg ("transfer: CANCEL\n");
            PutLineIntoResp2 (M ("Transfer cancelled (%s)"), RF);
            break;
            
        default:
            dmsg ("transfer: ERROR\n");
            PutLineIntoResp2 (M ("Error during transferring '%s'"), RF);
        }

    skip:
        PutLineIntoResp2 (M ("Skipped file '%s'"),
                          where == DOWN ? RF : (where == UP ? LF : ""));
        goto further;

    cancelled:
        trc = TR_CANCEL;
        goto further;
        
    further:
        if (hnd > 0) close (hnd);
        // remove 0 bytes long file when transfer has failed
        if (!(T[i].flags & TF_DIRECTORY) &&
            (T[i].size != 0 || T[i].flags & TF_HIDDEN) &&
            stat (LF, &st) == 0 &&
            st.st_size == 0) unlink (LF);
        
        ShowProgress (RF, LF, ind_type, where, 0., &tot, &cur, &interval_start);
        tot.tr.files += 1;
        tot.spent += clock1()-interval_start;
        interval_start = clock1 ();
        free (RF); free (LF);
        if (trc == TR_CANCEL) break;
    }

    if (noisy && !cmdline.batchmode)
    {
        if (options.transfer_complete_action[0] != '\0')
        {
            if (strcmp (options.transfer_complete_action, "bell") == 0)
                Bell (2);
            else
                fly_launch (options.transfer_complete_action, FALSE, FALSE);
        }

        if (options.transfer_pause)
        {
            RF = str_sjoin (T[nt-1].r_dir, T[nt-1].r_name);
            LF = str_sjoin (T[nt-1].l_dir, T[nt-1].l_name);
            ShowProgress (RF, LF, ind_type, where, -1., &tot, &cur, &interval_start);
            free (RF);
            free (LF);
            getkey (-1);
        }
        //set_view_mode (VIEW_REMOTE);
    }

    /* when uploading we must invalidate all directories which
     received files */
    if (where == UP)
    {
        /* first we create a list of unique directories */
        dirs = xmalloc (sizeof (char *) * nt);
        for (i=0; i<nt; i++)
            dirs[i] = T[i].r_dir;
        qsort (dirs, nt, sizeof(char *), cmp_str);
        ndirs = uniq (dirs, nt, sizeof(char *), cmp_str);
        /* invalidate directories. don't invalidate current directory */
        for (i=0; i<ndirs; i++)
            if (strcmp (RCURDIR.name, dirs[i]) != 0)
                invalidate_directory (dirs[i], FALSE);
    }

    PutLineIntoResp2 (M("Transfer done; average speed is %u bytes/sec"), 
                      (unsigned long)site.speed);
    set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
    if (where == DOWN) build_local_filelist (NULL);
    dmsg ("end of transfer()\n");
    return trc;
}

/* ------------------------------------------------------------- */

static int open_handle (int where, int action, char *LF)
{
    int   mode, hnd, i, ln;
    char  *p, *p1, *p2;
    struct stat st;

    mode = BINMODE;
    switch (where)
    {
    case LIST:
    case LSATTRS:
    case RETATTRS:
        mode |= O_WRONLY|O_CREAT|O_TRUNC;
        break;

    case DOWN:
        mode |= O_WRONLY;
        if (action == ACT_OVERWRITE) mode |= O_CREAT|O_TRUNC;;
        break;

    case UP:
    case SETATTRS:
        mode |= O_RDONLY;
        break;
    }

    // check for overwrite of existing file!
    if (where == DOWN && action == ACT_OVERWRITE && stat (LF, &st) == 0)
    {
        /* we create backup if backups are on, file is large and:
         - noninteractive mode;
         - interactive mode, user agreed
         */
        p1 = strdup (fl_str.yes); str_strip2 (p1, " ");
        p2 = strdup (fl_str.no);  str_strip2 (p2, " ");
        if (options.backups &&
            st.st_size > options.backups_limit &&
            (site.batch_mode ||
              !fly_ask (ASK_YN,
                        "  The file %s is %s bytes!  \n"
                        "  Select '%s' to overwrite it, '%s' to leave backup copy  ",
                        NULL, LF, pretty64(st.st_size), p1, p2)))
        {
            ln = strlen (LF) + 16;
            p = xmalloc (ln);
            for (i=1; ; i++)
            {
                snprintf1 (p, ln, "%s.%d", LF, i);
                if (access (p, F_OK) != 0) break;
            }
            rename (LF, p);
            free (p);
        }
        free (p1);
        free (p2);
    }
    
    hnd = open (LF, mode, 0666);
    if (hnd < 0)
    {
        if (errno == EACCES)
        {
            chmod (LF, 0600);
            hnd = open (LF, mode, 0666);
            if (hnd >= 0) return hnd;
        }

        PutLineIntoResp2 (M ("Cannot open file %s"), LF);
        return -1;
    }

    return hnd;
}

/* -------------------------------------------------------------
 returns:
 ERR_CANCELLED
 ERR_TRANSIENT
 ERR_AUTHENTICATE
 ERR_FATAL
 ERR_RESTFAIL
 */
static int issue_transfer_cmd (int where, int action, char *cmd, int64_t lsize, int *sock)
{
    int             sock1;
    int             rc, rsp, port;
    unsigned long   ip, h_ip;
    unsigned int    h_po;
    
    if (is_http_proxy)
    {
        if (action == ACT_REGET) return ERR_RESTFAIL;
        // connect to proxy
        sock1 = Connect2 (firewall.fwip, htons (options.fire_port));
        if (sock1 < 0) PutLineIntoResp2 (M ("Cannot connect to transfer data"));
        switch (sock1)
        {
        case ERR_FATAL:
        case ERR_CANCELLED: return sock1;
        }
        // send the command
        dmsg ("sending out: [%s]\n", cmd);
        rc = send (sock1, cmd, strlen (cmd), 0);
        if (rc < 0)
        {
            socket_close (sock1);
            return ERR_TRANSIENT;
        }
        if (where == DOWN || where == LIST)
        {
            rc = swallow_http_headers (sock1);
            if (rc) socket_close (sock1);
            switch (rc)
            {
            case ERR_CANCELLED:
            case ERR_TRANSIENT:
            case ERR_AUTHENTICATE:
            case ERR_SKIP:
            case ERR_FATAL:        return rc;
            }
        }
        *sock = sock1;
        return ERR_OK;
    }

    // establish data communication channel
    if (status.passive_mode)
    {
        rc = IssuePASVCommand (&ip, &port);
        switch (rc)
        {
            case ERR_TRANSIENT:
            case ERR_FATAL:
            case ERR_SKIP:
            case ERR_CANCELLED:  return rc;
        }
        sock1 = Connect2 (ip, port);
        if (sock1 < 0) PutLineIntoResp2 (M ("Cannot connect to transfer data"));
        switch (sock1)
        {
        case ERR_FATAL:
        case ERR_CANCELLED: return sock1;
        }
    }
    else
    {
        sock1 = BindListen (&port);
        switch (sock1)
        {
        case ERR_FATAL: return sock1;
        }
    newport:
        h_ip = ntohl (site.l_ip);
        h_po = ntohs (port);
        // tell remote where to communicate
        rc = SendToServer (TRUE, &rsp, NULL, "PORT %lu,%lu,%lu,%lu,%u,%u",
                           (h_ip/(256*256*256)) % 256,
                           (h_ip/(256*256))     % 256,
                           (h_ip/256)           % 256,
                           (h_ip)               % 256,
                           (h_po/256)           % 256,
                           (h_po)               % 256);
        if (rc == ERR_FIXPORT) goto newport;
        if (rc) socket_close (sock1);
        if (rsp == 5 || rsp == 4)
        {
            port_failed++;
            if (port_failed >= 5) status.passive_mode = TRUE;
            return ERR_TRANSIENT;
        }
        switch (rc)
        {
            case ERR_TRANSIENT:
            case ERR_FATAL:
            case ERR_SKIP:
            case ERR_CANCELLED:  return rc;
        }
    }

    if (where == DOWN && action == ACT_REGET)
    {
        // try to do reget if needed
        rc = SendToServer (FALSE, &rsp, NULL, "REST %s", printf64(lsize));
        if (rc) socket_close (sock1);
        switch (rc)
        {
        case ERR_TRANSIENT:
        case ERR_FATAL:
        case ERR_SKIP:
        case ERR_CANCELLED:  return rc;
        }
        if (rsp != 3)
        {
            PutLineIntoResp2 (M ("Reget isn't supported; transferring entire file"));
            socket_close (sock1);
            site.features.rest_works = FALSE;
            return ERR_RESTFAIL;
        }
    }

    // issue command to start transfer
    rc = SendToServer2 (FALSE, &rsp, NULL, cmd);
    if (rc) socket_close (sock1);
    switch (rc)
    {
    case ERR_TRANSIENT:
    case ERR_FATAL:
    case ERR_SKIP:
    case ERR_CANCELLED:  return rc;
    }
    if (where == UP && action == ACT_REGET && rsp == 5)
    {
        PutLineIntoResp2 (M ("Reget isn't supported; transferring entire file"));
        socket_close (sock1);
        site.features.appe_works = FALSE;
        return ERR_RESTFAIL;
    }
    if (rsp != 1)
    {
        dmsg ("response is not 1\n");
        socket_close (sock1);
        if (rsp == 4 && site.system_type != SYS_AS400) return ERR_TRANSIENT;
        return ERR_SKIP;
    }

    if (!status.passive_mode)
    {
        *sock = Accept (sock1);
        socket_close (sock1);
        if (*sock < 0)
        {
            //set_view_mode (VIEW_CONTROL);
            PutLineIntoResp2 (M ("Server did not establish data connection"));
            return *sock;
        }
    }
    else
    {
        *sock = sock1;
    }

    return ERR_OK;
}
        
/* ------------------------------------------------------------- */

static int transfer_data (int where, int sock, int hnd, char *LF, char *RF,
                          int ind_type, progress *tot, progress *cur,
                          double *interval_start)
{
    int             numbytes = 0, nb = 0, nbs, key, br, rc;
    int             bufsize, lopt, trc, blocking;
    struct timeval  tv;
    double          activity, now;
    fd_set          rdfs, wrfs;
    char            *bufin, *p;

    errno = 0;
    activity = clock1 ();

    // see what buffer system wants
    bufsize = 0;
    rc = 0;
#ifndef __BEOS__
    lopt = sizeof (bufsize);
    rc = getsockopt (sock, SOL_SOCKET, where == DOWN ? SO_RCVBUF : SO_SNDBUF, (void *) &bufsize, &lopt);
    dmsg ("getsockopt(SO_RCV/SNDBUF)=%d; bufsize = %d\n", rc, bufsize);
#endif
    if (rc || bufsize == 0) bufsize = 32*1024;
    // allocate transfer buffer
    bufin = malloc (bufsize);
        
    //dmsg ("transfer: main loop entered\n");
    br = 0;
    trc = -1;
    blocking = TRUE;
    SetBlockingMode (sock, blocking);
    do
    {
        /* control socket blocking mode: block if speed is good and
         do select() if speed is low */
        if (site.speed < 50000. && blocking == TRUE)
        {
            blocking = FALSE;
            SetBlockingMode (sock, blocking);
            dmsg ("speed is %g, switching to nonblocking mode\n", site.speed);
        }
        else if (site.speed > 50000. && blocking == FALSE)
        {
            blocking = TRUE;
            SetBlockingMode (sock, blocking);
            dmsg ("speed is %g, switching to blocking mode\n", site.speed);
        }

        //dmsg ("transfer: main loop 1\n");
        if (status.transfer_paused)
        {
            wait_a_bit (200000);
        }
        else
        {
            tv.tv_sec = 0; tv.tv_usec = 500000;
            switch (where)
            {
            case DOWN:
            case LIST:
            case LSATTRS:
            case RETATTRS:
                if (site.speed < 50000.)
                {
                    FD_ZERO (&rdfs);
                    FD_SET (sock, &rdfs);
                    rc = select (sock+1, &rdfs, NULL, NULL, &tv);
                    //dmsg ("transfer: select (read) returns %d; errno = %d\n", rc, errno);
                    if (rc < 0)
                    {
                        switch (errno)
                        {
                        case EINTR: /* do nothing, normal situation */
                            break;
                        case EAGAIN: /* socket is empty, wait a bit */
                            wait_a_bit (200000);
                            break;
                        default: /* transfer completed! */
                            trc = TR_OK;
                            dmsg ("trc is now TR_OK\n");
                            break;
                        }
                    }
                    //if (rc < 0) {trc = TR_OK; break; }
                    /* avoid trying to receive when select() did not indicate presence
                    of data */
                    if (rc <= 0) break;
                }

                nb = recv (sock, bufin, bufsize, 0);
                //dmsg ("transfer: received %d bytes on recv(); errno=%d\n", nb, errno);
                if (nb == 0) {trc = TR_OK; break;}
                if (nb < 0) 
                {
                    switch (errno)
                    {
                    case EINTR: /* do nothing, normal situation */
                       break;
                    case EAGAIN: /* socket is empty, wait a bit */
                       //dmsg ("sleeping\n");
                       wait_a_bit (200000);
                       break;
                    default: /* transfer completed! */
                       trc = TR_RETRY;
                       //dmsg ("trc is now TR_RETRY\n");
                       break;
                    }
                    break;
                }
                activity = clock1 ();

                numbytes = write (hnd, bufin, nb);
                if (nb != numbytes)
                {
                    if (!is_batchmode)
                        fly_ask_ok (ASK_WARN, M ("%s\nError writing to file (disk full?)"), LF);
                    trc = TR_CANCEL;
                    break;
                }

                cur->tr.bytes += numbytes;
                tot->tr.bytes += numbytes;
                break;

            case UP:
            case SETATTRS:
                nb = read (hnd, bufin, bufsize);
                //dmsg ("read(file) = %d\n", nb);
                if (nb <= 0) {trc = TR_OK; break;}
                br += nb;

                //dmsg ("speed = %f\n", site.speed);
                if (site.speed < 50000.)
                {
                    FD_ZERO (&wrfs);
                    FD_SET (sock, &wrfs);
                    errno = 0;
                    //dmsg ("entering select(write)... ");
                    rc = select (sock+1, NULL, &wrfs, NULL, &tv);
                    if (rc < 0)
                    {
                        if (errno == EINTR || errno == EAGAIN) break;
                        trc = TR_RETRY;
                        break;
                    }
                    if (rc == 0)
                    {
                        lseek (hnd, -nb, SEEK_CUR);
                        br -= nb;
                        break;
                    }
                }

                p = bufin;
                nbs = nb;
            send_again:
                errno = 0;
                numbytes = send (sock, p, nbs, 0);
                //dmsg ("send() returned %d (tried %d); errno = %d\n", numbytes, nbs, errno);
                if (numbytes != nbs)
                {
                    if (numbytes != -1)
                    {
                        nbs -= numbytes;
                        p += numbytes;
                    }
                    else
                    {
                        /* we got -1. analyze errno */
                        switch (errno)
                        {
                        case EINTR: /* do nothing, normal situation */
                            break;
#ifdef __MINGW32__
                        case 0:
#endif
                        case EAGAIN: /* socket is full, wait a bit */
                            wait_a_bit (200000);
                            break;
                        default: /* seems we got some kind of error */
                            trc = TR_RETRY;
                            //dmsg ("trc is now TR_RETRY\n");
                            break;
                        }
                    }
                    if (trc == -1)
                        goto send_again;
                }

                if (numbytes > 0) activity = clock1 ();
                cur->tr.bytes += nb;
                tot->tr.bytes += nb;
            }
        }
        if (trc != -1) break; // bail out if we are to exit the loop

        //dmsg ("transfer: main loop 2\n");
        do
        {
            key = getmessage (0);
            if (!cmdline.batchmode)
            {
                switch (key)
                {
                case 'q':
                case 'Q':
                case FMSG_BASE_MENU+KEY_STOP_TRANSFER:
                    if (ind_type != PROGRESS_SCREEN) break;
                    trc = TR_CANCEL;
                    break;

                case 's':
                case 'S':
                case FMSG_BASE_MENU+KEY_SKIP_TRANSFER:
                    if (ind_type != PROGRESS_SCREEN) break;
                    trc = TR_SKIP;
                    break;

                case 'r':
                case 'R':
                case FMSG_BASE_MENU+KEY_RESTART_TRANSFER:
                    if (ind_type != PROGRESS_SCREEN) break;
                    trc = TR_RETRY;
                    CloseControlConnection ();
                    break;

                case 'p':
                case 'P':
                case FMSG_BASE_MENU+KEY_PAUSE_TRANSFER:
                    if (!status.transfer_paused)  tot->spent += clock1()-*interval_start;
                    else                          *interval_start = clock1();
                    status.transfer_paused = !status.transfer_paused;
                    adjust_menu_status ();
                    break;

                case _Esc:
                    if (ind_type != PROGRESS_MSG) break;
                    trc = TR_CANCEL;
                    break;
                }
            }
        }
        while (trc == -1 && key != 0 && key != -1);
        now = clock1 ();
        ShowProgress (RF, LF, ind_type, where, now-activity, tot, cur, interval_start);

        //if (now-activity > 10000.) dmsg ("[%f sec]", now-activity);
        
        //dmsg ("@@@ now-activity=%f, options.data_timeout=%d\n", now-activity, options.data_timeout);
        if (((site.features.rest_works && now-activity > options.data_timeout) ||
             (!site.features.rest_works && now-activity > options.data_timeout*5)) &&
            (where == DOWN || where == LIST) &&
            ind_type != PROGRESS_MSG &&
            !status.transfer_paused &&
            !is_http_proxy
            )
        {
            dmsg ("data timeout (%f)!\n", now-activity);
            trc = TR_RETRY;
            CloseControlConnection ();
        }
    }
    while (trc == -1);
    
    //dmsg ("transfer: exiting main loop\n");
    ShowProgress (RF, LF, ind_type, where, 0., tot, cur, interval_start);
    free (bufin);
    
    if (is_http_proxy && where == UP) swallow_http_headers (sock);
    socket_close (sock);

    return trc;
}

/* ------------------------------------------------------------- */

static int transfer_restart (int where, trans_item T, char *LF, int64_t *restsize, int nt)
{
    time_t          to_mtime = 0, now;
    int64_t         to_size  = 0;
    int             j, k, decision;
    char            n1[28], n2[28];
    struct stat     st;
    struct tm       tm1, tm2;
    char            *actions[5], answers[128];

    *restsize = 0;
    dmsg ("transfer_restart: flags = %x\n", T.flags);
    if (T.flags & TF_OVERWRITE) return ACT_OVERWRITE;
    if (T.flags & TF_HIDDEN) return ACT_OVERWRITE;
    if (where == LIST || where == LSATTRS || where == SETATTRS || where == RETATTRS)
        return ACT_OVERWRITE;
    
    if (where == DOWN)
    {
        // local file exists?
        if (stat (LF, &st) < 0)
        {
            dmsg ("transfer restart: local file does not exist: OVERWRITE\n");
            return ACT_OVERWRITE;
        }
        else
        {
            dmsg ("local file has st.mtime = %lu\n", st.st_mtime);
            to_size  = st.st_size;
            to_mtime = st.st_mtime;
        }
    }
    else
    {
        // search for remote file
        for (j=0; j<site.ndir; j++)
        {
            if (strcmp (site.dir[j].name, T.r_dir) == 0) break;
        }
        if (j == site.ndir)
        {
            dmsg ("upload: remote dir not found: overwrite\n");
            return ACT_OVERWRITE;
        }
        for (k=0; k<site.dir[j].nfiles; k++)
        {
            if (strcmp (site.dir[j].files[k].name, T.r_name) == 0) break;
        }
        if (k == site.dir[j].nfiles)
        {
            dmsg ("upload: remote file not found: overwrite\n");
            return ACT_OVERWRITE;
        }
        to_size = site.dir[j].files[k].size;
        to_mtime = site.dir[j].files[k].mtime;
    }
    
    now = time (NULL);
    dmsg ("\ntransfer restart: (now %lu)\n"
          "to_size = %s, from_size = %s; \n"
          "to_time = %lu, from_time = %lu\n",
          now,
          printf64(to_size), printf64(T.size), to_mtime, T.mtime);
    
    *restsize = to_size;
    decision = ACT_OVERWRITE;

    /* SmartAppend. Decision is based on file sizes and dates.
     1. T.mtime < to_time: Source is older than target (target could be broken download)
     1.1. T.size > to_size: Source is larger than target -- do transfer restart
     1.2. T.size < to_size: Source is smaller than target -- do overwrite
     1.3. T.size = to_size: Source is identical to target -- skip
     2. T.mtime > to_time: Source is newer than target (target has changed since last transfer)
     Always overwrite
     3. When target has timestamp in the future we also overwrite it because this
     is not a broken transfer
     */
    dmsg ("T.mtime - to_mtime = %d, to_mtime - now = %d\n", T.mtime - to_mtime, to_mtime - now);
    if (((int)T.mtime - (int)to_mtime > (is_OS2 ? 1 : 0)) || ((int)to_mtime - (int)now > 0))
    //if (T.mtime - to_mtime > (is_OS2 ? 1 : 0) && to_mtime < now)
    {
        decision = ACT_OVERWRITE;
    }
    else
    {
        if (T.size == to_size)     decision = ACT_SKIP;
        else if (T.size > to_size) decision = ACT_REGET;
        else                       decision = ACT_OVERWRITE;
    }
    
    if (nt <= options.batch_limit && !is_batchmode)
    {
        // ask user
        tm1 = *localtime (&T.mtime);
        tm2 = *localtime (&to_mtime);
        strcpy (n1, pretty64 (T.size));
        strcpy (n2, pretty64 (to_size));
        
        actions[0] = "";
        actions[1] = M ("Overwrite");
        actions[2] = M ("Resume");
        actions[3] = M ("Skip");
        actions[4] = M ("Cancel");
        snprintf1 (answers, sizeof(answers), " %s \n %s \n %s \n %s ", actions[1], actions[2], actions[3], actions[4]);
        k = fly_ask (0, M ("Target file already exists!\n"
                           "Suggested action: %s\n"
                           "\n"
                           "Remote: %s in %s\n"
                           "Local:  %s in %s\n"
                           "\n"
                           "Source: %s bytes, %2d %s %4d  %02d:%02d:%02d\n"
                           "Target: %s bytes, %2d %s %4d  %02d:%02d:%02d"),
                     answers,
                     actions[decision],
                     T.r_name, T.r_dir,
                     T.l_name, T.l_dir,
                     n1,
                     tm1.tm_mday, mon2txt (tm1.tm_mon), tm1.tm_year+1900,
                     tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                     n2,
                     tm2.tm_mday, mon2txt (tm2.tm_mon), tm2.tm_year+1900,
                     tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
        switch (k)
        {
        case 1:  decision = ACT_OVERWRITE; break;
        case 2:  decision = ACT_REGET;     break;
        case 3:  decision = ACT_SKIP;      break;
        case 4:
        case 0:
        default: decision = ACT_CANCEL;    break;
        }
    }

    dmsg ("transfer_restart: decision = %d\n", decision);
    return decision;
}

/* ------------------------------------------------------------- */

int create_dir (char *d)
{
    char *p, *p1;
    int  rc, rsp;

    rc = set_wd (d, FALSE);
    if (rc == 0) return 0;

    rc = SendToServer (TRUE, &rsp, NULL, "MKD %s", d);
    if (rc != 0) return -1;

    if (rsp != 2)
    {
        p = strdup (d);
        p1 = strrchr (p, '/');
        if (p1 == NULL) {free (p); return -1;}
        *p1 = '\0';
        rc = create_dir (p);
        if (rc == 0)
        {
            rc = SendToServer (TRUE, &rsp, NULL, "MKD %s", d);
            if (rc == 0 && rsp != 2) rc = -1;
        }
        free (p);
        return rc;
    }
    
    return 0;
}

/* -------------------------------------------------------------
"retrieve_dir"
tries to recover as far as possible
*/
RC retrieve_dir (char *dirname)
{
    int            rc, i, a, prevmode;
    char           *buf, *p, tmpfile[MAX_PATH];
    //int          gf_total, gf_trans;
    trans_item     T;

    if (options.history_everything)
    {
        history_add (&site.u);
    }
    
    // Getting directory listing
    put_message (M ("Getting directory listing..."));
    tmpnam1 (tmpfile);
    p = strrchr (tmpfile, '/');
    *p = '\0';

    T.r_dir  = dirname;
    T.l_dir  = tmpfile;
    T.r_name = "";
    T.l_name = p+1;
    T.mtime  = 0;
    T.size   = 0;
    T.perm   = 0600;
    T.description = NULL;
    T.f = NULL;
    T.flags = TF_OVERWRITE;

    prevmode = status.binary;
    if (site.system_type == SYS_MVS_NATIVE && !is_http_proxy && status.binary == TRUE)
    {
        status.binary = FALSE;
        SetTransferMode ();
    }

    rc = transfer (LIST, &T, 1, FALSE);
    
    if (prevmode != status.binary)
    {
        status.binary = prevmode;
        SetTransferMode ();
    }
    
    // pretend everything OK if LIST failed --- NOT
    //if (rc == ERR_SKIP) rc = 0;
    if (rc) return rc;
    
    // Analyzing it
    *p = '/';
    if (load_file (tmpfile, &buf) <= 0)
    {
        //return ERR_FATAL;
        buf = strdup (" ");
    }
    remove (tmpfile);

    if (site.system_type == SYS_WINNTDOS && strstr (buf, "owner") && strstr (buf, "group"))
    {
        site.system_type = SYS_WINNT;
    }

    /*
    site.system_type = SYS_ISA2000;
    buf = strdup ("<HTML>\n"
                  "<HEAD>\n"
                  "<TITLE>FTP directory /// at 10.11.120.197. </TITLE>"
                  "</HEAD>\n"
                  "<BODY>\n"
                  "<H1>FTP directory /// at 10.11.120.197. </H1>\n"
                  "<HR>\n"
                  "<PRE>\n"
                  "01/02/80 12:00          &lt;DIR&gt; <A HREF=\"///../\">..</A>\n"
                  "02/01/02 03:25          &lt;DIR&gt; <A HREF=\"///a/\">a</A>\n"
                  "02/01/02 03:25          &lt;DIR&gt; <A HREF=\"///b/\">b</A>\n"
                  "02/01/02 03:21          6,681 <A HREF=\"///brazilian.mnu\">brazilian.mnu</A>\n"
                  "</PRE>"
                  "<HR>"
                  "</BODY>"
                  "</HTML>");
                  */

    /*
    site.system_type = SYS_MVS_NATIVE;
    
    buf = strdup (
" Name     VV.MM   Created       Changed      Size  Init   Mod   Id                  \n"
"LOG                                                                   \n"
"UTMPX                                                                 \n"
);
*/
/*
    buf = strdup (
"Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname\n"
"SESPUS 3380                                        VSAM DDIR\n"
"SESPUS 3380                                        VSAM DDIR.D\n"
"SESPUS 3380                                        VSAM DDIR.I\n"
"SESPUS 3380   2000/02/18  1   24  FB      80  3120  PO  DUY.TEST\n"
"SESPUS 3380   1970/05/04  1  150  VBA    255  1254  PS  IPCS.PRINT\n"
"SESPUS 3380   2000/02/09  1    5  FB      80  6160  PO  ISPF.ISPPROF\n"
"SESPUS 3380   2000/02/18  1    1  VB     256  6233  PS  LOG\n"
"SESPUS 3380   2000/02/16  1    1  VB     255  3120  PS  LOG.MISC\n"
"SESPUS 3380   2000/02/16  1    1  VB     255  3120  PS  LOG.MISC.BAK\n"
"SESPUS 3380   2000/02/11  1    1  VB     256  6233  PS  NEWFILE.DUY\n"
"SESPUS 3380   2000/02/16  1    8  VBA    125   129  PS  SPFLOG1.LIST\n"
"SESPUS 3380   2000/02/16  1    1  VBA    125   129  PS  SPFLOG1.LIST.BAK\n"
"SESPUS 3380   2000/02/18 16   18  FB      80  3120  PO  TPNS.TESTFILE\n"
                 );
*/
/*
    buf = strdup (
"Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname\n"
"SYSPP1 3390   2000/01/20  1    3  FB      80  3120  PO  BSCSYS.ISPTABL\n"
"Migrated                                                BSCSYS.ISPUSER\n"
"SYSPP2 3390   1999/12/07  1    5  FB      80  4240  PO  BSCSYS.SAS\n"
"SYSPP5 3390                                        VSAM DDIR\n"
"SYSPP5 3390                                        VSAM DDIR.D\n"
"SYSPP5 3390                                        VSAM DDIR.I\n"
"SYSPP1 3390   2000/01/20  3   11  FB      80  3120  PO  ISPF.PROFILE\n"
"Migrated                                                LOG.MISC\n"
"Migrated                                                RMFOS130.ADMGDF\n"
"Migrated                                                RMFOS130.ISPTABLE\n"
"SYSPP1 3390   2000/01/14  1   90  FB      80 27920  PO  SMPEJCL.DATA\n"
"SYSPP5 3390   2000/01/18  1    1  VA     125   129  PS  SPFLOG0.LIST\n"
"SYSPP5 3390   2000/01/20  1    1  VA     125   129  PS  SPFLOG1.LIST\n"
"SYSPP5 3390   2000/01/06  2    2  VA     125   129  PS  SPFLOG3.LIST\n"
"SYSPP5 3390   2000/01/07  1    1  VA     125   129  PS  SPFLOG4.LIST\n"
"SYSPP5 3390   2000/01/10  1    1  VA     125   129  PS  SPFLOG5.LIST\n"
"SYSPP5 3390   2000/01/11  1    1  VA     125   129  PS  SPFLOG6.LIST\n"
"SYSPP5 3390   2000/01/12  1    1  VA     125   129  PS  SPFLOG7.LIST\n"
"SYSPP4 3390   2000/01/14  1    1  VA     125   129  PS  SPFLOG8.LIST\n"
"SYSPP1 3390   2000/01/17  1    1  VA     125   129  PS  SPFLOG9.LIST\n"
"SYSPP1 3390   2000/01/14  1    1  FBA    133 13566  PS  SUPERC.LIST\n"
"SYSPP2 3390   2000/01/06  1    3  FBA     80  3120  PO  TUP.HDS\n"
"SYSPP4 3390   2000/01/20  1   90  FB      80 27920  PO  TUP.JCL\n"
"SYSPP4 3390   2000/01/06  1   90  FB      80 27920  PO  TUP.JCL.ATGEMME\n"
"SYSPP5 3390   2000/01/18  1   30  U     9076  9076  PO  TUP.LOD\n"
"R5TE11 3390   2000/01/06  1   15  FB      80 27920  PO  TUP.PAN\n"
"SYSPP4 3390   2000/01/20  2   75  FB      80 27920  PO  TUP.REX\n"
"SYSPP5 3390   2000/01/20  1  148  FB      80 27920  PO  TUP.SRC\n"
                  "SYSPP2 3390   2000/01/06  1   15  FBA     80  3120  PS  TUP.SYSOUT\n"
                 );
    site.system_type = SYS_MVS_NATIVE;
*/
    if (site.features.has_mlst)
    {
        analyze_listing (buf, dirname, SYS_MLST);
    }
    else
    {
        analyze_listing (buf, dirname, site.system_type);
    }
    //site.dir[site.cdir].name = chunk_put (site.chunks, dirname, -1);
    sort_remote_files (NULL);

    free (buf);
    
    // look for index files
    if (status.load_descriptions && !is_batchmode)
    {
        for (i=0; i<RNFILES; i++)
            if ((RFILE(i).flags & FILE_FLAG_REGULAR) &&
                isindexfile (RFILE(i).name)) break;

        if (i != RNFILES)
        {
            a = 1;
            if (RFILE(i).size >= options.desc_size_limit*1024 ||
                (site.speed > 0. && RFILE(i).size/site.speed > options.desc_time_limit))
            {
                a = fly_ask (0, M ("Caution: file '%s' containing file descriptions\n"
                                   "is %s bytes in length. Do you still want to download it?"),
                             M (" Yes \n No \n Never "),
                             RFILE(i).name,
                             pretty64(RFILE(i).size));
            }
            if (a == 3) status.load_descriptions = FALSE;
            if (a == 1) attach_descriptions (i);
        }
    }
    //set_view_mode (VIEW_REMOTE);
    //set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);

    return rc;
}

/* ------------------------------------------------------------- */

RC SetTransferMode (void)
{
    if (status.binary)
        return SendToServer (TRUE, NULL, NULL, "TYPE I");
    else
        return SendToServer (TRUE, NULL, NULL, "TYPE A");
}

/* ------------------------------------------------------------- */

RC set_wd (char *newdir, int noisy)
{
    int  rc, rsp;

    if (strcmp (newdir, site.u.pathname) == 0) return ERR_OK;

    if (is_http_proxy)
    {
        strcpy (site.u.pathname, newdir);
        rc = 0;
    }
    else
    {
        // kill terminating slash if present
        //l = strlen (newdir);
        //if (l >= 2 && newdir[l-1] == '/' && newdir[l-2] != ':') newdir[l-1] = '\0';
        if (noisy) put_message (M ("Changing directory to:\n'%s'"), newdir);
        rc = SendToServer (TRUE, &rsp, NULL, "CWD %s", newdir);
        if (rc == ERR_OK && rsp == 2 && site.features.unixpaths)
        {
            strcpy (site.u.pathname, newdir);
        }
        if (rc == ERR_OK && rsp != 2) rc = ERR_SKIP;
    }

    return rc;
}

/* ------------------------------------------------------------- */

void log_transfers (char *format, ...)
{
   va_list       args;
   FILE          *trnfile;

   if (!options.log_trans) return;

   trnfile = try_open (options.log_trans_name, "a");
   if (trnfile == NULL)
   {
       fly_ask_ok (ASK_WARN, M ("Warning: cannot write to log file '%s'"),
                   options.log_trans_name);
       return;
   }
   va_start (args, format);
   fprintf (trnfile, "%s ", datetime());
   vfprintf (trnfile, format, args);
   fclose (trnfile);
   va_end (args);
}

/* -------------------------------------------------------------
"traverse_directory" walks into subdirs traversing entire tree
*/
#if 0
RC traverse_directory (char *dir)
{
    int             i, d, rc;
    
    rc = change_dir (dir);
    if (rc) return rc;
    d = site.cdir;

    for (i=0; i<site.dir[d].nfiles; i++)
        if (site.dir[d].files[i].flags & FILE_FLAG_DIR
            && strcmp (site.dir[d].files[i].name, ".")
            && strcmp (site.dir[d].files[i].name, ".."))
        {
            rc = traverse_directory (site.dir[d].files[i].name);
            if (getkey (0) == 'q') rc = ERR_CANCELLED;
            if (rc == ERR_FATAL || rc == ERR_CANCELLED) break;
            set_dir_size (d, i);
        }
    
    change_dir ("..");
    return rc;
}

/* -------------------------------------------------------------
"traverse_directory" walks into subdirs traversing entire tree */
RC traverse_directory (char *dir, int mark)
{
    int             i, d, rc;

    rc = r_chdir (dir, mark);
    if (rc) return rc;
    d = site.cdir;

    for (i=0; i<site.dir[d].nfiles; i++)
    {
        if (site.dir[d].files[i].flags & FILE_FLAG_DIR
            && strcmp (site.dir[d].files[i].name, ".."))
        {
            rc = traverse_directory (site.dir[d].files[i].name, mark);
            if (getkey (0) == 'q') rc = ERR_CANCELLED;
            if (rc == ERR_FATAL || rc == ERR_CANCELLED) break;
            set_dir_size (d, i);
        }
    }

    rc = r_chdir ("..", FALSE);
    return rc;
}
#endif

/* -------------------------------------------------------------
 walks into subdirs traversing entire tree */
int cache_subtree (int dirno, int fileno)
{
    int i, d, rc, marked;
    char *p;

    /* sanity check: if this is not a subdirectory we don't do anything */
    if (!(site.dir[dirno].files[fileno].flags & FILE_FLAG_DIR)) return 0;

    /* see if this directory is marked */
    marked = FALSE;
    if (site.dir[dirno].files[fileno].flags & FILE_FLAG_MARKED) marked = TRUE;

    /* change to this directory and try to obtain its listing */
    p = str_sjoin (site.dir[dirno].name, site.dir[dirno].files[fileno].name);
    rc = cache_dir (p);
    if (rc && rc != ERR_SKIP)
    {
        free (p);
        return rc;
    }

    /* locate new directory among cached */
    d = find_directory (p);
    if (d < 0)
    {
        free (p);
        return ERR_SKIP;
    }

    rc = 0;
    for (i=0; i<site.dir[d].nfiles; i++)
    {
        /* skip things we are not interested in */
        if (!markable (site.dir[d].files[i])) continue;

        /* mark files if needed */
        if (marked) site.dir[d].files[i].flags |= FILE_FLAG_MARKED;

        /* walk subdirectories */
        if (site.dir[d].files[i].flags & FILE_FLAG_DIR)
        {
            rc = cache_subtree (d, i);
            if (rc == ERR_FATAL || rc == ERR_CANCELLED) break;
            set_dir_size (d, i);
        }
    }

    return rc;
}

/* ------------------------------------------------------------- */
RC get_dirname (char *dirname)
{
    int         rsp, rc, l;
    char        *p1, *p2, separator, buf[1024];
    
    put_message (M ("Querying current directory on remote..."));

    rc = SendToServer (TRUE, &rsp, buf, "PWD");
    if (rc) return rc;
    if (rsp == 2)
    {
        p1 = buf;

        if (strchr (buf, '"' ) != NULL) separator = '"';
        else  if (strchr (buf, '\'') != NULL) separator = '\'';
        else  return ERR_FATAL;

        while (*p1 != separator && *p1) p1++;
        if (*p1 == 0) return ERR_FATAL;
        p2 = p1+1;
        while (*p2 != separator && *p2) p2++;
        if (*p2 == 0) return ERR_FATAL;

        *p2 = 0;
        strcpy (dirname, p1+1);
        str_translate (dirname, '\\', '/');
        if (site.system_type == SYS_VMS_MADGOAT)
        {
            p1 = strstr (dirname, "/000000/");
            if (p1 != NULL) strcpy (dirname, p1+7);
        }
        if (site.system_type == SYS_VMS_UCX)
        {
            p1 = strstr (dirname, ":[");
            if (p1 != NULL)
            {
                strcpy (dirname, p1+2);
                p1 = strrchr (dirname, ']');
                if (p1 != NULL) *p1 = '\0';
            }
            str_translate (dirname, '.', '/');
        }
        if (site.system_type == SYS_MVS_NATIVE)
        {
            str_strip2 (dirname, "'.");
        }
        dmsg ("get_dirname: returning [%s]\n", dirname);
        // never ever return backslashes!
        str_translate (dirname, '\\', '/');
        // drive letters are always case insensitive
        if (strlen(dirname) > 1 && dirname[1] == ':')
            dirname[0] = tolower ((unsigned char)dirname[0]);
        // kill terminating slash if present
        l = strlen (dirname);
        if (l >= 2 && dirname[l-1] == '/' && dirname[l-2] != ':') dirname[l-1] = '\0';
        strcpy (site.u.pathname, dirname);
        return ERR_OK;
    }
    else
    {
        if (!is_batchmode)
            fly_ask_ok (ASK_WARN, M ("Cannot determine current directory on remote"));
        strcpy (dirname, ".");
        return ERR_OK;
    }
}

/* -------------------------------------------------------------
 IssuePASVCommand sends PASV command and returns port and IP address
 in network order
 returns
 0 -- OK,
 1 -- error (PASV isn't supported?)
*/

static RC IssuePASVCommand (unsigned long *ip, int *port)
{
    int             i, rc, rsp;
    char            *p, *p1, resp[64];
    unsigned char   h[6];
    u_short         port1;
    
    rc = SendToServer (TRUE, &rsp, resp, "PASV");
    if (rc) return rc;
    if (rsp != 2) return ERR_FATAL;
    
    p = resp + 4;
    while ((*p < '0' || *p > '9') && *p) p++;
    if (*p == 0) return ERR_FATAL; /* can't parse */
    
    for (i=0; i<6; i++)
    {
        p1 = p;
        while (*p1 && (*p1 >= '0' && *p1 <= '9')) p1++;
        if (*p1 == '\0' && i != 5) return ERR_FATAL; /* can't parse */
        *p1 = '\0';
        h[i] = atoi (p);
        p = p1 + 1;
    }
    memcpy (ip,     h,   4);
    memcpy (&port1, h+4, 2);
    *port = port1;
    //*ip = h[0] + h[1]*256 + h[2]*256*256 + h[3]*256*256*256;
    //*port = h[4]*256 + h[5];
    
    return ERR_OK;
}

/* ------------------------------------------------------------- */

static void ShowProgress (char *rfilename, char *lfilename, int ind_type,
                          int where, double stall, progress *tot,
                          progress *cur, double *interval_start)
{
    static int           stalled_window_name = 0, prev_percent = 0;
    static double        prev2 = 0., prev3 = 0.;
    
    double          now, updlimit, cur_eta, tot_eta, interval;
    double          tot_spent, cur_spent;
    progress_data   tot_re, cur_re;
    
    int             l1, l2, bar_len, percent, clr, is_download;
    char            s_stall[28], s_speed[28], bar1[256], bar2[256], slack[256], *msg;
    char            line1[100];
    char            s_bt_total[28], s_bt_trans[28], s_bt_skip[28], s_bt_left[28];
    char            s_tt_total[28], s_tt_spent[28], s_tt_left[28];
    char            s_bc_total[28], s_bc_trans[28], s_bc_skip[28], s_bc_left[28];
    char            s_tc_total[28], s_tc_spent[28], s_tc_left[28];

    updlimit = 0.3;
    if (site.speed > 10.) updlimit = 0.5;
    if (fl_opt.is_unix) updlimit = 1.0;
    if (site.speed > 100.) updlimit = 1.2;
    if (options.slowlink) updlimit = 3.0;

    now = clock1 ();
    interval = now - *interval_start;
    if (status.transfer_paused)
    {
        tot_spent = tot->spent;
        cur_spent = cur->spent;
    }
    else
    {
        tot_spent = tot->spent + interval;
        cur_spent = cur->spent + interval;
    }
    //dmsg ("interval = %f, tot: spent = %f, paused = %f\n", interval, tot_spent);

    // compute numbers
    tot_re.files = max1 (0, tot->to.files - tot->tr.files - tot->sk.files);
    tot_re.bytes = max1 (0, tot->to.bytes - tot->tr.bytes - tot->sk.bytes);
    cur_re.bytes = max1 (0, cur->to.bytes - cur->tr.bytes - cur->sk.bytes);
    percent = tot->to.bytes ?
        (int)(((double)tot->tr.bytes+(double)tot->sk.bytes)*100./(double)tot->to.bytes) : 0;
    if (percent > 100.0) percent = 100.0;

    // speed is measured in B/sec
    if (tot_spent > 0.) site.speed = (double)(tot->tr.bytes)/tot_spent;

    cur_eta = site.speed == 0. ? 0. : (unsigned long)(cur_re.bytes/site.speed);
    tot_eta = site.speed == 0. ? 0. : (unsigned long)(tot_re.bytes/site.speed);

    // change session title to reflect transfer progress
    is_download = where == DOWN || where == LIST || where == RETATTRS || where == LSATTRS;
    if (now-prev2 > 2. || percent >= 99)
    {
        if (stall > options.stalled_interval)
        {
            set_window_name ("%s %s %u%% %s %s",
                             is_download ? "->" : "<-",
                             fl_opt.is_unix ? "stalled" : M ("stalled"),
                             percent,
                             is_download ? "from" : "to",
                             site.u.hostname);
            stalled_window_name = TRUE;
        }
        else
        {
            set_window_name ("%s %u%% at %.2f KB/s %s %s",
                             is_download ? "->" : "<-",
                             percent, site.speed/1000.,
                             is_download ? "from" : "to",
                             site.u.hostname);
            stalled_window_name = FALSE;
        }
        prev2 = now;
        prev_percent = percent;
    }
    
    if (stall < options.stalled_interval && stalled_window_name)
    {
            set_window_name ("%s %u%% at %.2f KB/s %s %s",
                             is_download ? "->" : "<-",
                             percent, site.speed/1000.,
                             is_download ? "from" : "to",
                             site.u.hostname);
            stalled_window_name = FALSE;
    }
    // check for too often updates
    if (now - prev3 < updlimit && cur_re.bytes != 0 && /*tot_re.bytes != 0 &&*/ stall > -0.5) return;
    prev3 = now;
    
    // message-type progress indicator
    if (ind_type == PROGRESS_MSG)
    {
        if (tot->to.bytes == 0)
            put_message (M ("%7d bytes received\npress ESC to stop"), tot->tr.bytes);
        else
            put_message (M ("%7d bytes received\n%7d bytes remain\npress ESC to stop"), tot->tr.bytes,
                         tot->to.bytes-tot->tr.bytes-tot->sk.bytes);
        return;
    }

    // format sizes and times
    strcpy (s_stall,     make_time((unsigned long)(stall < 0. ? 0. : stall)));
    strcpy (s_speed,     pretty ((unsigned long)site.speed));
    strcpy (s_bt_total,  pretty64 (tot->to.bytes));
    strcpy (s_bt_trans,  pretty64 (tot->tr.bytes));
    strcpy (s_bt_skip,   pretty64 (tot->sk.bytes));
    strcpy (s_bt_left,   pretty64 (tot_re.bytes));
    strcpy (s_tt_total,  make_time ((unsigned long)(tot_spent+tot_eta)));
    strcpy (s_tt_spent,  make_time ((unsigned long)tot_spent));
    strcpy (s_tt_left,   make_time ((unsigned long)tot_eta));
    strcpy (s_bc_total,  pretty64 (cur->to.bytes));
    strcpy (s_bc_trans,  pretty64 (cur->tr.bytes));
    strcpy (s_bc_skip,   pretty64 (cur->sk.bytes));
    strcpy (s_bc_left,   pretty64 (cur_re.bytes));
    strcpy (s_tc_total,  make_time ((unsigned long)(cur_spent+cur_eta)));
    strcpy (s_tc_spent,  make_time ((unsigned long)cur_spent));
    strcpy (s_tc_left,   make_time ((unsigned long)cur_eta));

    // make bars
    // window width: max video_hsize-2 at max,
    //               min max1 (strlen (rfilename), strlen (lfilename))
    bar_len = min1 (video_hsize()-7,
                    max1 (60,
                          max1 (strlen (rfilename),
                                strlen (lfilename)
                               )
                         )
                   );
    l1 = tot->to.bytes == 0 ? bar_len :
        min1 (bar_len, (unsigned)
              ( (double)(tot->tr.bytes+tot->sk.bytes)*(double)bar_len/(double)tot->to.bytes ));
    l2 = cur->to.bytes == 0 ? bar_len :
        min1 (bar_len, (unsigned)
              ( (double)(cur->tr.bytes+cur->sk.bytes)*(double)bar_len/(double)cur->to.bytes ));
    memset (bar1, fl_sym.fill4, l1);
    memset (bar1+l1, fl_sym.fill1, bar_len-l1);
    bar1[bar_len] = '\0';
    memset (bar2, fl_sym.fill4, l2);
    memset (bar2+l2, fl_sym.fill1, bar_len-l2);
    bar2[bar_len] = '\0';
    memset (slack, ' ', sizeof(slack));
    slack[max1(0, min1(sizeof(slack)-1, bar_len-57))] = '\0';

    update (0);
    if (cmdline.batchmode)
    {
        msg = "";
    }
    else
    {
        if (stall == -1.0)
            msg = M ("Transfer completed; press any key to continue");
        else if (stall == -2.0)
            msg = M ("Communicating with server...");
        else
            msg = status.transfer_paused ? M ("Paused; press P to continue") : 
                  M ("S/Q/R/P - Skip/Quit/Restart/Pause");
    }

    if (tot->to.files == 1)
    {
        snprintf1 (line1, sizeof(line1), M ("%9u%% done (%s, %s)"), percent,
                 status.binary ? M ("binary") : M ("ASCII"),
                 is_download ? M ("%s - downloading...") : M ("%s - uploading..."));
    }
    else
    {
        snprintf1 (line1, sizeof(line1), M ("%9u%% done (%s, %s) (%d of %d files)"), percent,
                 status.binary ? M ("binary") : M ("ASCII"),
                 is_download ? M ("%s - downloading...") : M ("%s - uploading..."),
                 tot->tr.files, tot->to.files);
    }
    
    clr = fl_clr.dbox_back;
    if (stall == -1.0 && tot->tr.bytes+tot->sk.bytes != tot->to.bytes)
    {
        fl_clr.dbox_back = fl_clr.dbox_back_warn;
    }
    
    if (tot->to.files == 1)
    {
        put_message
            (" %s \n"
             " %10s %s \n"
             " %10s %s \n"
             "\n"
             " %s \n"
             " %s \n"
             " %-30s %s %13s (%9s) \n"
             " %-30s %s %13s (%9s) \n"
             " %-30s %s %13s \n"
             " %-30s %s %13s (%9s) \n"
             " %s \n"
             " %s ",
             line1,
             s_speed, M ("bytes/sec"),
             s_stall, M ("seconds since last packet"),
             rfilename, lfilename,
             M ("Total"),       slack, s_bt_total, s_tt_total,
             M ("Transferred"), slack, s_bt_trans, s_tt_spent,
             M ("Skipped"),     slack, s_bt_skip,
             M ("Left"),        slack, s_bt_left, s_tt_left,
             bar2,
             msg
            );
    }
    else
    {
        put_message
            (" %s \n"
             " %10s %s \n"
             " %10s %s \n"
             "\n"
             " %-15s %s %13s (%9s) \n"
             " %-15s %s %13s (%9s) \n"
             " %-15s %s %13s \n"
             " %-15s %s %13s (%9s) \n"
             " %s \n"
             "\n"
             " %s \n"
             " %s \n"
             " %-15s %s %13s (%9s) \n"
             " %-15s %s %13s (%9s) \n"
             " %-15s %s %13s \n"
             " %-15s %s %13s (%9s) \n"
             " %s \n"
             " %s ",
             line1,
             s_speed, M ("bytes/sec"),
             s_stall, M ("seconds since last packet"),
             M ("Total"),       slack, s_bt_total, s_tt_total,
             M ("Transferred"), slack, s_bt_trans, s_tt_spent,
             M ("Skipped"),     slack, s_bt_skip,
             M ("Left"),        slack, s_bt_left, s_tt_left,
             bar1,
             rfilename, lfilename,
             M ("Total"),       slack, s_bc_total, s_tc_total,
             M ("Transferred"), slack, s_bc_trans, s_tc_spent,
             M ("Skipped"),     slack, s_bc_skip,
             M ("Left"),        slack, s_bc_left, s_tc_left,
             bar2,
             msg
            );
    }
    
    if (stall == -1.0 && tot->tr.bytes+tot->sk.bytes != tot->to.bytes)
    {
        fl_clr.dbox_back = clr;
    }
}

/* ------------------------------------------------------------- */

RC cache_file (int nd, int nf)
{
    char           tmpfile[MAX_PATH], *p;
    int64_t        lfile;
    int            rc1, rc2, prevmode;
    trans_item     T;
    struct stat    st;

    dmsg ("caching file %s\n", site.dir[nd].files[nf].name);

    // form the name of the local file
    tmpnam1 (tmpfile);
    p = strrchr (tmpfile, '/');
    *p = '\0';

    T.r_dir = site.dir[nd].name;
    T.l_dir = tmpfile;
    T.r_name = site.dir[nd].files[nf].name;
    T.l_name = p+1;
    T.mtime = site.dir[nd].files[nf].mtime;
    T.size   = site.dir[nd].files[nf].size;
    T.perm   = 0600;
    T.description = NULL;
    T.f = &(site.dir[nd].files[nf]);
    T.flags = 0;

    prevmode = status.binary;
    if (site.system_type == SYS_MVS_NATIVE && !is_http_proxy && status.binary == TRUE)
    {
        status.binary = FALSE;
        SetTransferMode ();
    }

    rc1 = transfer (DOWN, &T, 1, FALSE);
    *p = '/';

    if (prevmode != status.binary)
    {
        status.binary = prevmode;
        SetTransferMode ();
    }

    if (rc1)
    {
        remove (tmpfile);
        return ERR_FATAL;
    }

    // fix the size which might be actually different from reported by server!
    rc2 = stat (tmpfile, &st);
    if (rc2)
    {
        remove (tmpfile);
        return ERR_FATAL;
    }
    
    site.dir[nd].files[nf].size = st.st_size;
    
    lfile = load_file (tmpfile, &site.dir[nd].files[nf].cached);
    remove (tmpfile);

    return ERR_OK;
}

/* ------------------------------------------------------------- */

RC attach_descriptions (int nf)
{
    int    rc, i;
    char   *d;

    // load descriptions into file
    if (RFILE(nf).cached == NULL)
    {
        rc = cache_file (site.cdir, nf);
        if (rc) return rc;
        if (RFILE(nf).cached == NULL || RFILE(nf).cached[0] == '\0')
            return ERR_FATAL;
    }

    // place description
    str_parseindex (RFILE(nf).cached);
    for (i=0; i<RNFILES; i++)
    {
        d = str_finddesc (RFILE(i).name, RFILE(i).size);
        //d = str_find_description (RFILE(nf).cached, RFILE(i).name, RFILE(i).size);
        if (d != NULL)
        {
            RFILE(i).desc = chunk_put (site.chunks, d, -1);
            free (d);
        }
    }

    return ERR_OK;
}

/* -------------------------------------------------------------
 retrieves file given by url and stores it in the current directory
 returns: ERR_OK/ERR_FATAL/ERR_SKIP/ERR_CANCELLED
 */

int do_get (char *url)
{
    url_t  u;
    char   *file, path[2048];
    int    i, rc;
    trans_item  T;

    // look where we are
    parse_url (url, &u);
    if (u.pathname[0] == '\0') return ERR_FATAL;
    file = strrchr (u.pathname, '/');
    if (file == NULL) return ERR_FATAL;
    *file = '\0';
    if (file == u.pathname) // file from the root directory?
        strcpy (path, "/");
    else
        str_scopy (path, u.pathname);
    file++;
    if (file[0] == '\0') return ERR_FATAL;

    // try to login and get to specified directory
    // check whether we're connecting to the same site
    rc = Login (&u);
    if (rc) return ERR_SKIP;
    
    //set_view_mode (VIEW_REMOTE);
    update (1);

    // look for specified file
    for (i=0; i<RNFILES; i++)
    {
        if (strcmp (RFILE(i).name, file) == 0) break;
    }

    T.r_dir = RCURDIR.name;
    T.l_dir = local.dir.name;
    T.r_name = file;
    T.l_name = strdup (file);
    T.mtime = i == RNFILES ? time(NULL) : RFILE(i).mtime;
    T.size   = i == RNFILES ? 0 : RFILE(i).size;
    T.perm   = i == RNFILES ? 0644 : RFILE(i).rights;
    T.description = i == RNFILES ? NULL : RFILE(i).desc;
    T.f = i == RNFILES ? NULL : &(RFILE(i));
    T.flags = 0;
    if (options.lowercase_names &&
        str_lastspn (T.l_name, "abcdefghijklmnopqrstuvwxyz") == NULL)
        str_lower (T.l_name);

    rc = transfer (DOWN, &T, 1, FALSE);
    
    free (T.l_name);
    
    return rc;
}

/* -------------------------------------------------------------
 puts file given by url from the current directory
 returns: ERR_OK/ERR_FATAL/ERR_SKIP/ERR_CANCELLED */
int do_put (char *url)
{
    url_t  u;
    char   *file, path[2048];
    int    i, rc;
    trans_item T;

    // look where we should go
    parse_url (url, &u);
    if (u.pathname[0] == '\0') return ERR_FATAL;
    file = strrchr (u.pathname, '/');
    if (file == NULL) return ERR_FATAL;
    *file = '\0';
    if (file == u.pathname) // file from the root directory?
        strcpy (path, "/");
    else
        str_scopy (path, u.pathname);
    file++;
    if (file[0] == '\0') return ERR_FATAL;

    // try to login and get to specified directory
    rc = Login (&u);
    if (rc) return ERR_SKIP;
        
    //set_view_mode (VIEW_REMOTE);
    update (1);

    // look for local file
    build_local_filelist (NULL);
    for (i=0; i<LNFILES; i++)
    {
        if (strcmp (LFILE(i).name, file) == 0) break;
    }
    if (i == LNFILES) return ERR_SKIP;

    T.r_dir = RCURDIR.name;
    T.l_dir = local.dir.name;
    T.r_name = LFILE(i).name;
    T.l_name = LFILE(i).name;
    T.mtime  = LFILE(i).mtime;
    T.size   = LFILE(i).size;
    T.perm   = LFILE(i).rights;
    T.description = LFILE(i).desc;

    rc = transfer (UP, &T, 1, FALSE);

    return rc;
}

/* ------------------------------------------------------------- */

static int swallow_http_headers (int sock)
{
    char             *header, *p, *p1, *p2;
    int              n, rc, nh = 1024, http_code = 0, authenticate, cancelled;
    fd_set           rdfs;
    struct timeval   tv;

    header = malloc (nh);
    cancelled = FALSE;
    authenticate = FALSE;
    
next_header:
    p = header;
    memset (header, 0, nh);
    while (1)
    {
        // first we wait for data
        do
        {
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            FD_ZERO (&rdfs);
            FD_SET (sock, &rdfs);
            rc = select (sock+1, &rdfs, NULL, NULL, &tv);
            if (rc < 0 || getkey (0) == _Esc) {free (header); return ERR_CANCELLED;}
        }
        while (rc == 0);
        // read next byte
        n = recv (sock, p, 1, 0);
        if (n <= 0) {free (header); return ERR_TRANSIENT;}
        // found end of HTTP headers?
        if (p - header == 1 && *(p-1) == '\r' && *p == '\n') break;
        // found end of header line?
        if (p - header >= 1 && *(p-1) == '\r' && *p == '\n')
        {
            *(p-1) = '\0';
            PutLineIntoResp1 ("%s", header);
            dmsg ("header line: %s\n", header);
            // check for return code
            if (str_headcmp (header, "HTTP/") == 0)
            {
                if ((p1 = strchr (header, ' ')) != NULL && (p2 = strchr (p1+1, ' ')) != NULL)
                {
                    *p2 = '\0';
                    // assign code!
                    http_code = atoi (p1+1);
                }
            }
            p1 = strdup (header);
            str_lower (p1);
            if (!http_authenticate && !cancelled && !str_headcmp (p1, "proxy-authenticate: "))
            {
                http_authenticate = TRUE;
                if (firewall.userid[0] == '\0' || firewall.password[0] == '\0')
                {
                    if (entryfield (M ("Enter your FIREWALL userid:"), firewall.userid,
                                    sizeof (firewall.userid), 0) == PRESSED_ESC ||
                        entryfield (M ("Enter your FIREWALL password:"), firewall.password,
                                    sizeof (firewall.password), LI_INVISIBLE) == PRESSED_ESC)
                    {
                        cancelled = TRUE;
                    }
                }
                authenticate = TRUE;
            }
            if (strstr (p1, "squid") != NULL)
            {
                site.system_type = SYS_SQUID;
            }
            else if (strstr (p1, "netscape-proxy") != NULL)
            {
                site.system_type = SYS_NSPROXY;
            }
            else if (strstr (p1, "bordermanager") != NULL)
            {
                site.system_type = SYS_BORDERMANAGER;
            }
            else if (strstr (p1, "apache") != NULL)
            {
                site.system_type = SYS_APACHE;
            }
            else if (strstr (p1, "inktomi") != NULL)
            {
                site.system_type = SYS_INKTOMI;
            }
            else if (str_headcmp (p1, "via: 1.1 ") == 0)
            {
                if (strchr (p1+9, ' ') == NULL)
                    site.system_type = SYS_ISA2000;
            }
            free (p1);
            goto next_header;
        }
        p++;
        // increase buffer if necessary
        if (p - header == nh)
        {
            nh += 1024;
            header = realloc (header, nh);
            p = header + nh - 1024;
        }
    }
    
    free (header);

    dmsg ("http code: %d\n", http_code);
    if (cancelled) return ERR_CANCELLED;
    if (authenticate) return ERR_AUTHENTICATE;
    if (http_code != 200 && http_code != 0) return ERR_SKIP;
    return 0;
}

/* ------------------------------------------------------------- */

static void transfer_bfs_attrs (int where, trans_item *t)
{
    int          i, j, rc, nd, na, fh, nl, bytes;
    char         **str, tmpfile[MAX_PATH], LF[2048], *p, *p1, *p2;
    char         buffer[1024];
    trans_item   T;
    befs_attr    *attrs;
    struct stat  st;
    
    T = *t;
    dmsg ("entered transfer_bfs_attrs (where=%d)\n", where);

    if (where == DOWN)
    {
        // download. see if file on server has attributes
        if (site.features.has_bfs1 == TRUE)
        {
            // temporary file to hold the attribute list
            tmpnam1 (tmpfile);
            p = strrchr (tmpfile, '/');
            *p = '\0';
            T.l_dir = tmpfile;
            T.l_name = p+1;
            // download the attribute list
            rc = transfer (LSATTRS, &T, 1, FALSE);
            *p = '/';
            // no attributes or transfer failed?
            nl = load_textfile (tmpfile, &str);
            remove (tmpfile);
            if (nl < 0) return;
            // create attrs[] array and fill it
            attrs = malloc (sizeof (befs_attr) * nl);
            na = 0;
            for (i=0; i<nl; i++)
            {
                // response format:
                // <type> <length> <name>
                //        p1       p2
                p1 = strchr (str[i], ' ');
                if (p1 == NULL) continue;
                *p1++ = '\0';
                p2 = strchr (p1, ' ');
                if (p2 == NULL) continue;
                *p2++ = '\0';
                attrs[na].type = strtol (str[i], NULL, 16);
                attrs[na].name = strdup (p2);
                attrs[na].value = NULL; // not yet here...
                attrs[na].length = atol (p1);
                na++;
            }
            free (str[0]);
            free (str);
            // now we try to download attributes...
            for (i=0; i<na; i++)
            {
                dmsg ("getting %s, %d bytes\n", attrs[i].name, attrs[i].length);
                // we don't have to get attributes of zero length (if they exist)
                if (attrs[i].length == 0)
                {
                    attrs[i].value = strdup ("");
                    continue;
                }
                // download the attribute value
                tmpnam1 (tmpfile);
                p = strrchr (tmpfile, '/');
                *p = '\0';
                snprintf1 (buffer, sizeof(buffer), "%s %s", t->r_name, attrs[i].name);
                T.r_name = buffer;
                T.l_dir = tmpfile;
                T.l_name = p+1;
                rc = transfer (RETATTRS, &T, 1, FALSE);
                *p = '/';
                bytes = load_file (tmpfile, &p);
                remove (tmpfile);
                dmsg ("got %d bytes, wanted %d bytes\n", bytes, attrs[i].length);
                if (bytes != attrs[i].length)
                {
                    if (bytes > 0) free (p);
                    continue;
                }
                attrs[i].value = p;
                dmsg ("attr download succeeded\n");
            }
            // clean those attributes which we failed to retrieve
            dmsg ("%d attributes before cleanup\n", na);
            for (i=0; i<na; i++)
            {
                while (attrs[i].value == NULL && i < na)
                {
                    free (attrs[i].name);
                    for (j=i; j<na-1; j++)
                    {
                        attrs[j] = attrs[j+1];
                    }
                    na--;
                }
            }
            dmsg ("%d attributes after cleanup\n", na);
        }
        else
        {
            // seek file on server with same name and attached suffix
            nd = find_directory (T.r_dir);
            dmsg ("found directory %d\n", nd);
            if (nd == -1) return; // not found. hmm....
            // search target
            p = str_join (T.r_name, ATTR_SUFFIX);
            for (i=0; i<site.dir[nd].nfiles; i++)
            {
                if (strcmp (site.dir[nd].files[i].name, p) == 0) break;
            }
            free (p);
            if (i == site.dir[nd].nfiles) return;
            // download that file
            tmpnam1 (tmpfile);
            p = strrchr (tmpfile, '/');
            *p = '\0';
            T.r_dir = site.dir[nd].name;
            T.l_dir = tmpfile;
            T.r_name = site.dir[nd].files[i].name;
            T.l_name = p+1;
            // download the attribute list
            rc = transfer (DOWN, &T, 1, FALSE);
            if (p != NULL) *p = '/';
            na = read_attrfile (tmpfile, &attrs);
            remove (tmpfile);
        }
        if (na <= 0) return;
        // have attributes? attach'em!
        strcpy   (LF, t->l_dir);
        str_translate (LF, '\\', '/');
        str_cats (LF, t->l_name);
        dmsg ("attaching %d attributes to %s\n", na, LF);
        bfs_attach (LF, attrs, na);
        destroy_attrs (attrs, na);
    }
    else if (where == UP)
    {
        // upload
        strcpy   (LF, t->l_dir);
        str_translate (LF, '\\', '/');
        str_cats (LF, t->l_name);
        na = bfs_fetch (LF, &attrs);
        dmsg ("fetched %d attributes; has_bfs1=%d\n", na, site.features.has_bfs1);
        if (na <= 0) return; // nothing to worry about....
        
        if (site.features.has_bfs1 == TRUE)
        {
            dmsg ("upload: has_bfs=TRUE\n");
            tmpnam1 (tmpfile);
            p1 = strdup (tmpfile);
            p = strrchr (tmpfile, '/');
            *p = '\0';
            T.l_dir  = tmpfile;
            T.l_name = p+1;
            for (i=0; i<na; i++)
            {
                fh = open (p1, BINMODE|O_WRONLY|O_CREAT|O_TRUNC, 0600);
                if (fh < 0) break;
                rc = write (fh, attrs[i].value, attrs[i].length);
                close (fh);
                dmsg ("wrote %d chars into %s\n", rc, p1);
                snprintf1 (buffer, sizeof(buffer), "%s %s %x", t->r_name, attrs[i].name, attrs[i].type);
                T.r_name = buffer;
                T.size = attrs[i].length;
                dmsg ("calling SETATTRS");
                transfer (SETATTRS, &T, 1, FALSE);
            }
            remove (p1);
            free (p1);
        }
        else
        {
            // upload file to server with same name and attached suffix
            tmpnam1 (tmpfile);
            rc = write_attrfile (tmpfile, attrs, na);
            if (rc < 0)
            {
                remove (tmpfile);
                return;
            }
            rc = stat (tmpfile, &st);
            if (rc)
            {
                remove (tmpfile);
                return;
            }
            p = strrchr (tmpfile, '/');
            *p = '\0';
            T.r_name = str_join (T.r_name, ATTR_SUFFIX);
            T.l_dir = tmpfile;
            T.l_name = p+1;
            T.size = st.st_size;
            // upload the attribute file
            rc = transfer (UP, &T, 1, FALSE);
            *p = '/';
            free (T.r_name);
            remove (tmpfile);
        }
        destroy_attrs (attrs, na);
    }
}

/* compose command to initiate transfer */
static char *compose_command (int where, trans_item T, int action)
{
    char *cmd, *cmdprefix, *p, h1[128], buf[128], flags[16];

    cmd = malloc (2048);
    
    if (is_http_proxy)
    {
        // 1. GET/PUT request
        if (where == DOWN || where == LIST) cmdprefix = "GET ";
        else if (where == UP)               cmdprefix = "PUT ";
        else                                return NULL;
        strcpy (cmd, cmdprefix);
        p = compose_url (site.u, T.r_name, TRUE, site.system_type != SYS_NSPROXY);
        if (p == NULL) return NULL;
        strcat (cmd, p);
        free (p);
        strcat (cmd, " HTTP/1.0\n");
        p = compose_url (site.u, T.r_name, FALSE, site.system_type != SYS_NSPROXY);
        PutLineIntoResp3 ("%s%s HTTP/1.0", cmdprefix, p);
        free (p);
        // 2. Host:
        strcat (cmd,  "Host: ");
        strcat (cmd, site.u.hostname);
        strcat (cmd, "\n");
        PutLineIntoResp3 ("Host: %s", site.u.hostname);
        // 3. Authentication (if needed)
        if (http_authenticate)
        {
            snprintf1 (buf, sizeof(buf), "%s:%s", firewall.userid, firewall.password);
            p = base64_encode ((unsigned char *)buf, -1);
            strcat (cmd, "Proxy-Authorization: Basic ");
            strcat (cmd, p);
            strcat (cmd, "\n");
            PutLineIntoResp3 ("Proxy-Authorization: Basic *");
            free (p);
        }
        // 4. Content-type for upload
        if (where == UP)
        {
            strcat (cmd, "Content-Type: application/octet-stream\n");
            PutLineIntoResp3 ("Content-Type: application/octet-stream");
        }
        // 5. Content-Length for upload
        if (where == UP)
        {
            snprintf1 (h1, sizeof(h1), "Content-Length: %s", printf64(T.size));
            strcat (cmd, h1);
            strcat (cmd, "\n");
            PutLineIntoResp3 (h1);
        }
        // 6. User-Agent
        snprintf1 (h1, sizeof(h1), "User-Agent: %s", options.user_agent);
        strcat (cmd, h1);
        strcat (cmd, "\n");
        PutLineIntoResp3 (h1);
        // 7. Cache-control when specified
        if (options.flush_http_cache)
        {
            strcat (cmd, "Cache-control: no-cache\n");
            PutLineIntoResp3 ("Cache-control: no-cache");
            strcat (cmd, "Pragma: no-cache\n");
            PutLineIntoResp3 ("Pragma: no-cache");
        }
        // 8. Disable keepalives
        strcat (cmd, "Connection: close\n");
        PutLineIntoResp3 ("Connection: close");
        // 9. Terminate request
        strcat (cmd, "\n");
    }
    else
    {
        switch (where)
        {
        case LIST:
            if (site.features.has_mlst)
            {
                strcpy (cmd, "MLSD");
            }
            else
            {
                strcpy (cmd, "LIST");
                if (site.system_type == SYS_UNIX && status.use_flags)
                {
                    flags[0] = '\0';
                    if (options.show_dotfiles) strcat (flags, "a");
                    if (status.resolve_symlinks) strcat (flags, "L");
                    if (flags[0] != '\0')
                    {
                        strcat (cmd, " -");
                        strcat (cmd, flags);
                    }
                }
            }
            break;

        case DOWN:
            strcpy (cmd, "RETR ");
            strcat (cmd, T.r_name);
            break;

        case UP:
            strcpy (cmd, "STOR ");
            if (site.system_type == SYS_AS400 && options.as400_put) strcpy (cmd, "PUT ");
            if (action == ACT_REGET) strcpy (cmd, "APPE ");
            strcat (cmd, T.r_name);
            break;

        case LSATTRS:
            strcpy (cmd, "SITE LSAT ");
            strcat (cmd, T.r_name);
            break;

        case SETATTRS:
            strcpy (cmd, "SITE SATT ");
            strcat (cmd, T.r_name);
            break;

        case RETATTRS:
            strcpy (cmd, "SITE RATT ");
            strcat (cmd, T.r_name);
            break;
        }
    }
    
    return cmd;
}

static int get_mtime (trans_item *T)
{
    char rsp_buf[1024], *p;
    int  rc, rsp;
    time_t t;

    /* don't attempt MTDM if:
     - MDTM is switched off
     - MDTM does not work;
     - there's nowhere to assign;
     - exact time has already been obtained */
    if (!options.use_MDTM ||
        !site.features.has_mdtm ||
        T->f == NULL ||
        T->f->flags & FILE_FLAG_MDTM) return 0;

    /* try to change to directory where file is */
    rc = set_wd (T->r_dir, FALSE);
    if (rc) return rc;

    /* send a command */
    rc = SendToServer (TRUE, &rsp, rsp_buf, "MDTM %s", T->r_name);
    if (rc) return rc;

    /* check FTP response (usually 213) */
    if (rsp == 5) site.features.has_mdtm = FALSE;
    if (rsp != 2) return -1;

    /* response should look like:
     ftp> quote MDTM freebsd-install
     213 20021203085840
     reported time is UTC!
     */
    p = strchr (rsp_buf, ' ');
    if (p == NULL) return -1;

    t = parse_date_time (DATE_FMT_7, p+1, NULL);
    if (t != 0)
    {
        T->mtime = t;
        T->f->mtime = t;
        T->f->flags |= FILE_FLAG_MDTM;
    }
    dmsg ("%s/%s now has mtime=%u\n", T->r_dir, T->l_dir, T->mtime);

    return 0;
}

/* write/upload file properties */
static void set_properties (int where, trans_item T, char *LF)
{
    int             rc, rc1, rsp;
    struct          utimbuf ut;
    struct tm       tm1;
    struct stat     st;

    /* we only need to do that for file transfers, not attribute
     transfers or listing transfers */
    if (where != DOWN && where != UP) return;
    
    if (where == DOWN && options.descfile != NULL && T.description != NULL)
        write_description (T.l_name, T.description, options.descfile);

    // set timestamp
    if (where == DOWN && options.preserve_timestamp_download)
    {
        if (is_time1980 && T.mtime < T_01_01_1980)
        {
            //ut.actime  = local2gm (T_01_01_1980);
            ut.actime  = T_01_01_1980;
        }
        else
        {
            //ut.actime  = local2gm (T.mtime);
            ut.actime  = T.mtime;
        }
        ut.modtime = ut.actime;
        dmsg ("setting mtime of %s to %u\n", LF, ut.modtime);
        utime (LF, &ut);
        stat (LF, &st);
        dmsg ("mtime now is %u, local %u, was %u\n", st.st_mtime,
              gm2local (st.st_mtime), T.mtime);
    }

    // SITE UTIME file access-time mod-time
    if (where == UP && options.preserve_timestamp_upload &&
        site.features.utime_works && !is_http_proxy)
    {
        // 19901231095959
        tm1 = *gmtime (&T.mtime);
        SendToServer (TRUE, &rsp, NULL,
                      "SITE UTIME %s %04u%02u%02u%02u%02u%02u %04u%02u%02u%02u%02u%02u %04u%02u%02u%02u%02u%02u UTC",
                      T.r_name,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
        if (rsp != 2) site.features.utime_works = FALSE;
    }

    if (where == DOWN && fl_opt.is_unix && options.preserve_permissions_download)
    {
        rc1 = chmod (LF, T.perm);
    }

    if (where == UP &&
        options.preserve_permissions_upload &&
        site.system_type == SYS_UNIX &&
        !is_anonymous &&
        site.features.chmod_works &&
        (T.perm & 0777) != 0644 &&
        fl_opt.is_unix &&
        !is_http_proxy)
    {
        rc = SendToServer (TRUE, &rsp, NULL, "SITE CHMOD %o %s", T.perm & 0777, T.r_name);
        if (rc == 0 && rsp == 5) site.features.chmod_works = FALSE;
    }

    dmsg ("deciding whether to transfer BFS attributes:\n");
    dmsg ("where = %d, l_name=%s, r_name=%s\n", where, T.l_name, T.r_name);
    dmsg ("site.features.has_bfs1=%d, options.use_bfs_files=%d\n",
          site.features.has_bfs1, options.use_bfs_files);
    if ((where == DOWN || where == UP) &&
        str_tailcmp (T.l_name, ATTR_SUFFIX) &&
        str_tailcmp (T.r_name, ATTR_SUFFIX) &&
        !is_http_proxy &&
        (site.features.has_bfs1 == TRUE || options.use_bfs_files))
    {
        transfer_bfs_attrs (where, &T);
    }
}

/* -------------------------------------------------------------
-- r_chdir:
tries to change into specified directory, retrieves filelist if successful
-- parameters:
dirname - must specify directory name
-- returns:
0 if success, 1 if error, 2 if cancelled

Logic behind r_chdir:
a) HTTP proxy
   I. Empty dirname.
      error: return ERR_FATAL
   II. Dirname specified
      try to change
      1) error: return error
      2) ok: return ok (we're in the new directory!)
b) not HTTP proxy
   I. Empty dirname
*/
RC r_chdir (char *dirname, int mark)
{
    int     prevdir, i, do_cd, rc;
    char    newdir[2048], *oldpath=NULL, *p;

    do_cd = TRUE;

    if (dirname[0] != '\0') oldpath = strdup (RCURDIR.name);

    if (is_http_proxy)
    {
        // HTTP proxy (can't do "cd")
        if (dirname[0] == '\0') return 0;

        if (dirname[0] == '/' || (strlen (dirname) > 1 && dirname[1] == ':'))
        {
            strcpy (newdir, dirname);
        }
        else
        {
            strcpy (newdir, RCURDIR.name);
            str_cats (newdir, dirname);
            sanify_pathname (newdir);
        }
        do_cd = FALSE;
    }
    else
    {
        // not HTTP proxy
        if (site.features.unixpaths)
        {
            // if dirname is empty, find out what current directory is
            if (dirname[0] == '\0')
            {
                rc = get_dirname (newdir);
                if (rc) return rc;
                do_cd = FALSE;
            }
            // if ~username is used, try to change to it and then see where we landed
            // or this is first invocation and path isn't absolute
            else if (dirname[0] == '~' || (dirname[0] == '/' && dirname[1] == '~') ||
                     (site.ndir == 1 && dirname[0] != '/' && (strlen (dirname) > 1 && dirname[1] != ':')))
            {
                if (dirname[0] == '/' && dirname[1] == '~')
                    rc = set_wd (dirname+1, TRUE);
                else
                    rc = set_wd (dirname, TRUE);
                //if (rc) return rc;
                rc = get_dirname (newdir);
                if (rc) return rc;
                do_cd = FALSE;
            }
            // if absolute path is given, use it
            else if (dirname[0] == '/' || (strlen (dirname) > 1 && dirname[1] == ':'))
            {
                strcpy (newdir, dirname);
            }
            // otherwise just concatenate current and dirname
            else
            {
                strcpy (newdir, RCURDIR.name);
                str_cats (newdir, dirname);
            }

            // come and make me holy again
            sanify_pathname (newdir);
        }
        else
        {
            // site.features.unixpaths == FALSE.
            if (dirname[0] != '\0')
            {
                put_message (M ("Changing directory to:\n%s"), newdir);
                rc = set_wd (dirname, TRUE);
                if (site.ndir != 1 &&
                    (rc == ERR_FATAL || rc == ERR_CANCELLED || rc == ERR_TRANSIENT)) return rc;
            }
            rc = get_dirname (newdir);
            if (rc) return rc;
            do_cd = FALSE;
        }
    }

    // check whether we have new directory in the cache
    // (searching amongst already read dirs)
    for (i=1; i<site.ndir; i++)
    {
        if (strcmp (site.dir[i].name, newdir) == 0)
        {
            site.cdir = i;
            set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
            sort_remote_files (RCURFILE.name);
            if (oldpath != NULL && str_headcmp (oldpath, RCURDIR.name) == 0)
            {
                p = oldpath + strlen (RCURDIR.name);
                if (*p == '/') p++;
                adjust_cursor (&RCURDIR, p);
            }
            else
                adjust_cursor (&RCURDIR, NULL);
            return 0;
        }
    }

    // try to go to newly guessed directory
    if (do_cd)
    {
        //if (options.autocontrol) display.view_mode = VIEW_CONTROL;
        put_message (M ("Changing directory to\n'%s'"), newdir);
        rc = set_wd (newdir, TRUE);
        if (rc)
        {
            get_dirname (newdir);
            return rc;
        }
        if (site.system_type == SYS_IBMOS2FTPD ||
            site.system_type == SYS_WINNTDOS ||
            site.system_type == SYS_OS2NEOLOGIC)
        {
            rc = get_dirname (newdir);
            if (rc) return rc;
        }
    }

    // check (AGAIN!) whether we have new directory in the cache
    // (searching amongst already read dirs)
    for (i=1; i<site.ndir; i++)
    {
        if (strcmp (site.dir[i].name, newdir) == 0)
        {
            site.cdir = i;
            set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
            sort_remote_files (RCURFILE.name);
            if (oldpath != NULL && str_headcmp (oldpath, RCURDIR.name) == 0)
            {
                p = oldpath + strlen (RCURDIR.name);
                if (*p == '/') p++;
                adjust_cursor (&RCURDIR, p);
            }
            else
                adjust_cursor (&RCURDIR, NULL);
            return 0;
        }
    }

    // new directory, need to obtain listing
    prevdir = site.cdir;
    //prevdirname = str_sjoin ();
    rc = retrieve_dir (newdir);
    if (rc)
    {
        site.cdir = prevdir;
    }
    else
    {
        site.cdir = find_directory (newdir);
        if (site.cdir < 0)
        {
            site.cdir = prevdir;
        }
        else
        {
            if (oldpath != NULL && str_headcmp (oldpath, RCURDIR.name) == 0)
            {
                p = oldpath + strlen (RCURDIR.name);
                if (*p == '/') p++;
                adjust_cursor (&RCURDIR, p);
            }
            else
                adjust_cursor (&RCURDIR, NULL);
            if (mark)
            {
                set_marks (RCURDIR.name);
            }
        }
    }
    //free (prevdirname);
    set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);

    if (oldpath != NULL) free (oldpath);

    display.quicksearch[0] = '\0';
    return rc;
}

/* -------------------------------------------------------------
-- change_dir:
tries to change into specified directory, retrieves filelist if successful
-- parameters:
dirname - must specify directory name
-- returns:
0 if success, 1 if error, 2 if cancelled

Logic behind change_dir:
a) HTTP proxy
   I. Empty dirname.
      error: return ERR_FATAL
   II. Dirname specified
      try to change
      1) error: return error
      2) ok: return ok (we're in the new directory!)
b) not HTTP proxy
   I. Empty dirname */
RC change_dir (char *dirname)
{
    int     prevdir, i, do_cd, rc;
    char    newdir[2048];

    do_cd = TRUE;

    if (is_http_proxy)
    {
        // HTTP proxy (can't do "cd")
        if (dirname[0] == '\0') return 0;
        
        if (dirname[0] == '/' || (strlen (dirname) > 1 && dirname[1] == ':'))
        {
            strcpy (newdir, dirname);
        }
        else
        {
            strcpy (newdir, RCURDIR.name);
            str_cats (newdir, dirname);
            sanify_pathname (newdir);
        }
        do_cd = FALSE;
    }
    else
    {
        // not HTTP proxy
        if (site.features.unixpaths)
        {
            // if dirname is empty, find out what current directory is
            if (dirname[0] == '\0')
            {
                rc = get_dirname (newdir);
                if (rc) return rc;
                do_cd = FALSE;
            }
            // if ~username is used, try to change to it and then see where we landed
            // or this is first invocation and path isn't absolute
            else if (dirname[0] == '~' || (dirname[0] == '/' && dirname[1] == '~') ||
                     (site.ndir == 1 && dirname[0] != '/' && (strlen (dirname) > 1 && dirname[1] != ':')))
            {
                if (dirname[0] == '/' && dirname[1] == '~')
                    rc = set_wd (dirname+1, TRUE);
                else
                    rc = set_wd (dirname, TRUE);
                //if (rc) return rc;
                rc = get_dirname (newdir);
                if (rc) return rc;
                do_cd = FALSE;
            }
            // if absolute path is given, use it
            else if (dirname[0] == '/' || (strlen (dirname) > 1 && dirname[1] == ':'))
            {
                strcpy (newdir, dirname);
            }
            // otherwise just concatenate current and dirname
            else
            {
                strcpy (newdir, RCURDIR.name);
                str_cats (newdir, dirname);
            }

            // come and make me holy again
            sanify_pathname (newdir);
        }
        else
        {
            // site.features.unixpaths == FALSE.
            if (dirname[0] != '\0')
            {
                put_message (M ("Changing directory to:\n'%s'"), newdir);
                rc = set_wd (dirname, TRUE);
                if (site.ndir != 1 &&
                    (rc == ERR_FATAL || rc == ERR_CANCELLED || rc == ERR_TRANSIENT)) return rc;
            }
            rc = get_dirname (newdir);
            if (rc) return rc;
            do_cd = FALSE;
        }
    }
    
    // check whether we have new directory in the cache
    // (searching amongst already read dirs)
    for (i=1; i<site.ndir; i++)
    {
        if (strcmp (site.dir[i].name, newdir) == 0)
        {
            site.cdir = i;
            set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
            sort_remote_files (RCURFILE.name);
            return 0;
        }
    }
    
    // try to go to newly guessed directory
    if (do_cd)
    {
        //if (options.autocontrol) display.view_mode = VIEW_CONTROL;
        put_message (M ("Changing directory to:\n'%s'"), newdir);
        rc = set_wd (newdir, TRUE);
        if (rc)
        {
            get_dirname (newdir);
            return rc;
        }
        if (site.system_type == SYS_IBMOS2FTPD || site.system_type == SYS_WINNTDOS ||
            site.system_type == SYS_OS2NEOLOGIC)
        {
            rc = get_dirname (newdir);
            if (rc) return rc;
        }
    }

    // check (AGAIN!) whether we have new directory in the cache
    // (searching amongst already read dirs)
    for (i=1; i<site.ndir; i++)
    {
        if (strcmp (site.dir[i].name, newdir) == 0)
        {
            site.cdir = i;
            set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);
            sort_remote_files (RCURFILE.name);
            return 0;
        }
    }
    
    // new directory, need to obtain listing
    prevdir = site.cdir;
    rc = retrieve_dir (newdir);
    if (rc)
    {
        site.cdir = prevdir;
    }
    else
    {
        site.cdir = find_directory (newdir);
        if (site.cdir < 0)
        {
            site.cdir = prevdir;
        }
    }
    set_window_name ("%s - %s", site.u.hostname, RCURDIR.name);

    display.quicksearch[0] = '\0';
    return rc;
}

/* -------------------------------------------------------------
 -- cache_dir: makes sure that specified directory (full path)
 is cached. returns 0 on success, negative values on errors */
RC cache_dir (char *dirname)
{
    int     i, rc;
    char    newdir[2048];

    /* check whether we have new directory in the cache
     (searching amongst already read dirs) */
    for (i=1; i<site.ndir; i++)
    {
        if (strcmp (site.dir[i].name, dirname) == 0) return 0;
    }

    /* try to change to this directory */
    rc = set_wd (dirname, TRUE);
    if (rc) return rc;

    rc = get_dirname (newdir);
    if (rc) return rc;

    if (strcmp (newdir, dirname) != 0) return -1;
    
    /* new directory, need to obtain listing */
    //prevdir = site.cdir;
    rc = retrieve_dir (dirname);
    //site.cdir = prevdir;

    return rc;
}
