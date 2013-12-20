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
    int files, bytes;
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
static int create_dir (int nsite, char *d);
static RC IssuePASVCommand (int nsite, unsigned long *ip, int *port);
static void ShowProgress (int nsite, int ns2,
                          char *rfilename, char *lfilename, int ind_type,
                          int where, double stall, progress *tot,
                          progress *cur, double *interval_start);
static void ShowProgressFXP (int nsite, int ns2, double stall,
                             char *rfilename, char *lfilename,
                             progress *tot, double *interval_start);
static int swallow_http_headers (int nsite, int sock);
static int transfer_data (int where, int nsite, int sock, int hnd, char *LF, char *RF,
                          int ind_type, progress *tot, progress *cur,
                          double *interval_start);
static int transfer_fxp (int nsite, int ns2, char *LF, char *RF,
                         progress *tot, double *interval_start);
static int transfer_restart (int where, int nsite, int ns2, trans_item T, char *LF, int64_t *restsize, int nt);
static int open_handle (int where, int nsite, int action, char *LF);
static int issue_transfer_cmd (int where, int nsite, int ns2, int action, char *cmd, char *cmd2, int64_t lsize, int *sock);
static void transfer_bfs_attrs (int where, int nsite, trans_item *T);
static char *compose_command (int where, int nsite, trans_item T, int action);
static void set_properties (int where, int nsite, int ns2, trans_item T, char *LF);

static void dump_progress (progress tot, progress cur)
{
    dmsg (" tot: to: files = %d, bytes = %s\n"
          "      tr: files = %d, bytes = %s\n"
          "      sk: files = %d, bytes = %s\n"
          " cur: to: files = %d, bytes = %s\n"
          "      tr: files = %d, bytes = %s\n"
          "      sk: files = %d, bytes = %s\n",
          tot.to.files, printf64(tot.to.bytes),
          tot.tr.files, printf64(tot.tr.bytes),
          tot.sk.files, printf64(tot.sk.bytes),
          cur.to.files, printf64(cur.to.bytes),
          cur.tr.files, printf64(cur.tr.bytes),
          cur.sk.files, printf64(cur.sk.bytes));
}

/* ------------------------------------------------------------- */

#define IS_OS2 (fl_opt.platform == PLATFORM_OS2_X || fl_opt.platform == PLATFORM_OS2_X11 || \
    fl_opt.platform == PLATFORM_OS2_VIO || fl_opt.platform == PLATFORM_OS2_PM)

RC transfer (int where, int nsite, int ns2, trans_item *T, int nt, int noisy)
{
    int        rc, hnd = -1, ind_type;
    char       *cmd, *cmd2, *RF, *LF, *p, *what;
    struct     stat st;
    int        sock=-1, sock1 = 0, rsp, rc2, rsp2;
    //, is_listing=FALSE;
    int        i, trc, action;
    int64_t    lsize;
    progress   tot, cur;
    double     interval_start;

    if (nt <= 0) return 0;

    dmsg ("[john] entered transfer0: where = %d, nt = %d. Items to transfer:\n", where, nt);
    for (i=0; i<nt; i++)
    {
        dmsg ("[john] r_dir = [%s], r_name = [%s], l_dir = [%s], l_name = [%s], "
              "time = %lu, size = %lu, perms = %o, flags = %x\n",
              T[i].r_dir, T[i].r_name, T[i].l_dir, T[i].l_name,
              T[i].mtime, T[i].size, T[i].perm, T[i].flags);
    }

    /* initialize progress data */
    tot.to.files = nt;
    tot.to.bytes = 0;
    for (i=0; i<nt; i++) tot.to.bytes += T[i].size;
    tot.tr.files = 0;
    tot.tr.bytes = 0;
    tot.sk = tot.tr;
    tot.spent = 0.;
    //is_listing = T[0].r_name[0] == '\0';
    if (where == FXP)                      ind_type = PROGRESS_FXP;
    else if (where == DOWN || where == UP) ind_type = PROGRESS_SCREEN;
    else                                   ind_type = PROGRESS_MSG;
    port_failed = 0;

    //put_message (MSG(M_STARTING_TRANSFER));
    switch (where)
    {
    case LIST:                                    what = "listing"; break;
    case LSATTRS: case SETATTRS: case RETATTRS:   what = "BFS attributes"; break;
    default:
    case DOWN: case UP: case FXP:                 what = "file";
    }
    put_message ("   Starting transfer of %s from %s      ", what, site[nsite].u.hostname);

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
                if (site[nsite].features.has_size)
                {
                    rc = SendToServer (TRUE, &rsp, buf, "SIZE %s", T[i].r_name);
                    if (rc) continue;
                    if (rsp != 2)
                    {
                        site[nsite].features.has_size = FALSE;
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
        dmsg ("transfer(): 2\n");
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

        if (where != FXP && make_subtree (T[i].l_dir) < 0)
        {
            PutLineIntoResp (RT_COMM, nsite, MSG(M_CANT_CREATE_DIRECTORY), T[i].l_dir);
            goto further;
        }

        // find out whether we need to restart/skip/overwrite
        action = transfer_restart (where, nsite, ns2, T[i], LF, &lsize, nt);
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
            goto skip;
        }
        if (action == ACT_REGET &&
            (is_http_proxy(nsite) ||
             (where == DOWN && !site[nsite].features.rest_works) ||
             (where == UP && !site[nsite].features.appe_works) ||
             (where == FXP && (!site[nsite].features.rest_works || !site[ns2].features.appe_works)) ||
             site[nsite].system_type == SYS_AS400 ||
             site[nsite].system_type == SYS_VMS_MULTINET
            )
           ) action = ACT_OVERWRITE;

        if (where != FXP)
        {
            hnd = open_handle (where, nsite, action, LF);
            if (where == DOWN && hnd < 0/* && IS_OS2*/)
            {
                // try shortened version of file name if possible
                p = str_shorten (T[i].l_name);
                if (p != NULL)
                {
                    strcpy (LF, T[i].l_dir);
                    str_translate (LF, '\\', '/');
                    str_cats (LF, p);
                }
                hnd = open_handle (where, nsite, action, LF);
            }
            if (hnd < 0)
            {
                if (!is_batchmode)
                {
                    if (where == DOWN)
                        fly_ask_ok (ASK_WARN, MSG(M_CANT_OPEN_FOR_WRITING), LF);
                    else
                        fly_ask_ok (ASK_WARN, MSG(M_CANT_OPEN_FOR_READING), LF);
                }
                goto skip;
            }
            dmsg ("opened %d\n", hnd);
        }

        // change directory on remote
        dmsg ("transfer(): 2\n");
        rc = set_wd (nsite, T[i].r_dir, FALSE);
        if (rc)
        {
            if (where == DOWN) goto skip;
            // try to create remote directory
            rc = create_dir (nsite, T[i].r_dir);
            if (rc) goto skip;
            rc = set_wd (nsite, T[i].r_dir, FALSE);
            if (rc) goto skip;
        }

        if (where == FXP)
        {
            rc = set_wd (ns2, T[i].l_dir, FALSE);
            if (rc)
            {
                // try to create remote directory
                rc = create_dir (ns2, T[i].l_dir);
                if (rc) goto skip;
                rc = set_wd (ns2, T[i].l_dir, FALSE);
                if (rc) goto skip;
            }
        }

        // compose transfer command
    retry:
    retry_auth:
        dmsg ("transfer(): 3\n");

        cmd = compose_command (where, nsite, T[i], action);
        if (cmd == NULL) goto skip;

        cmd2 = malloc (2048);
        strcpy (cmd2, "nonsense");
        if (where == FXP)
        {
            strcpy (cmd2, "STOR ");
            if (site[ns2].system_type == SYS_AS400 && options.as400_put) strcpy (cmd2, "PUT ");
            if (action == ACT_REGET) strcpy (cmd2, "APPE ");
            strcat (cmd2, T[i].l_name);
        }

        rc = issue_transfer_cmd (where, nsite, ns2, action, cmd, cmd2, lsize, &sock);
        free (cmd);
        free (cmd2);
        switch (rc)
        {
        case ERR_TRANSIENT:
            goto retry;
        case ERR_FATAL:
            if (where != FXP) close (hnd);
            goto skip;
        case ERR_SKIP:
            if (where != FXP) close (hnd);
            goto skip;
        case ERR_CANCELLED:
            if (where != FXP) close (hnd);
            goto cancelled;
        case ERR_AUTHENTICATE:
            goto retry_auth;
        case ERR_RESTFAIL:
            tot.tr.bytes -= cur.tr.bytes;
            cur.tr.bytes = 0;
            action = ACT_OVERWRITE;
            if (where != FXP)
            {
                close (hnd);
                hnd = open_handle (where, nsite, action, LF);
            }
            goto retry;
        }

        if (where != FXP && action == ACT_REGET)
        {
            lseek (hnd, lsize, SEEK_SET);
            cur.sk.bytes += lsize-cur.tr.bytes;
            tot.sk.bytes += lsize-cur.tr.bytes;
        }

        dmsg ("transfer(): 9\n");
        /*********************************************************/
        if (where != FXP)
        {
            trc = transfer_data (where, nsite, sock, hnd, LF, RF, ind_type, &tot, &cur, &interval_start);
            dmsg ("transfer_data() returned %d\n", trc);
        }
        else
        {
            trc = transfer_fxp (nsite, ns2, LF, RF, &tot, &interval_start);
            dmsg ("transfer_fxp() returned %d\n", trc);
        }
        /*********************************************************/

        if (is_http_proxy(nsite))
        {
            if (where == UP)
            {
                if (trc == TR_RETRY) trc = TR_SKIP;
            }
        }
        else
        {
            // see what server has to say
            ShowProgress (nsite, ns2, RF, LF, ind_type, where, -2.0, &tot, &cur, &interval_start);
            rc = SendToServer (nsite, FALSE, &rsp, NULL, NULL);
            if (rc) rsp = -1;
            if (where == FXP)
            {
                rc2 = SendToServer (ns2, FALSE, &rsp2, NULL, NULL);
                if (rc2) rsp2 = -1;
            }
        }
        if (where != FXP)
        {
            dmsg ("closing %d\n", hnd);
            close (hnd);
        }

        // check whether we got the entire file
        if (where != FXP)
        {
            dmsg ("comparing sizes: reget=%s, trans=%s, tsize=%s\n",
                  printf64(cur.sk.bytes), printf64(cur.tr.bytes), printf64(cur.to.bytes));
            if (trc == TR_OK &&
                !is_http_proxy(nsite) &&
                status.binary &&
                (cur.sk.bytes + cur.tr.bytes < cur.to.bytes) &&
                (rsp != 2 || where == UP)
               )
            {
                dmsg ("now willing to retry\n");
                trc = TR_RETRY;
            }
        }
        else
        {
            if (trc == TR_OK && (rsp != 2 || rsp2 != 2)) trc = TR_RETRY;
            if (rsp == 2 && rsp2 == 2)
            {
                tot.tr.bytes += T[i].size;
                site[nsite].speed = T[i].size / (clock1() - interval_start);
            }
        }

        switch (trc)
        {
        case TR_OK:
            dmsg ("transfer: OK\n");
            if (where == DOWN || where == UP)
                log_transfers ("%s (%6lu B/s) - %s:%s\n", printf64(T[i].size), (unsigned long)site[nsite].speed, site[nsite].u.hostname, RF);
            set_properties (where, nsite, ns2, T[i], LF);
            //if (T[i].f != NULL) T[i].f->flags &= ~FILE_FLAG_MARKED;
            goto further;
        case TR_RETRY:
            dmsg ("transfer: RETRY; site[nsite].features.rest_works=%d, appe_works=%d\n",
                  site[nsite].features.rest_works, site[nsite].features.appe_works);
            dump_progress (tot, cur);
            if ((where == DOWN && site[nsite].features.rest_works) ||
                (where == UP && site[nsite].features.appe_works))
            {
                tot.sk.bytes -= cur.sk.bytes;
                cur.sk.bytes = 0;
            }
            if ((where == DOWN && !site[nsite].features.rest_works) ||
                (where == UP && !site[nsite].features.appe_works))
            {
                tot.tr.bytes -= cur.tr.bytes;
                cur.tr.bytes = 0;
            }
            dump_progress (tot, cur);
            if (where == UP)
            {
                dmsg ("upload: re-reading directory\n");
                rc = retrieve_dir (nsite, RN_CURDIR.name, site[nsite].cdir);
                dmsg ("rc = %d\n", rc);
                if (rc) goto skip;
            }
            if (where == FXP)
            {
                dmsg ("upload: re-reading directory 2\n");
                rc = retrieve_dir (ns2, site[ns2].dir[site[ns2].cdir].name, site[ns2].cdir);
                dmsg ("rc = %d\n", rc);
                if (rc) goto skip;
            }
            dmsg ("determining whether to restart transfer\n");
            action = transfer_restart (where, nsite, ns2, T[i], LF, &lsize, 999999);
            dmsg ("decision: %d\n", action);
            goto restart;
            break;
        case TR_SKIP:
            dmsg ("transfer: SKIP\n");
            PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_TRANSFER_SKIPPED), RF);
            break;
        case TR_CANCEL:
            dmsg ("transfer: CANCEL\n");
            PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_TRANSFER_CANCELLED), RF);
            break;
        default:
            dmsg ("transfer: ERROR\n");
            PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_TRANSFER_ERROR), RF);
        }

    skip:
        PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_TRANSFER_SKIPPED), where == DOWN ? RF : LF);
        goto further;

    cancelled:
        trc = TR_CANCEL;
        goto further;
        break;

    further:
        // remove 0 bytes long file when transfer has failed
        if (T[i].size != 0 && stat (LF, &st) == 0 && st.st_size == 0) unlink (LF);

        ShowProgress (nsite, ns2, RF, LF, ind_type, where, 0., &tot, &cur, &interval_start);
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
            ShowProgress (nsite, ns2, RF, LF, ind_type, where, -1., &tot, &cur, &interval_start);
            free (RF);
            free (LF);
            getkey (-1);
        }
    }

    PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_TRANSFER_DONE), (unsigned long)site[nsite].speed);
    set_window_name ("%s - %s", site[nsite].u.hostname, RN_CURDIR.name);
    if (where == DOWN)
    {
        l_chdir (V_LEFT, NULL);
        l_chdir (V_RIGHT, NULL);
    }

    for (i=0; i<nt; i++)
    {
        free (T[i].r_dir); free (T[i].r_name);
        free (T[i].l_dir); free (T[i].l_name);
    }
    dmsg ("end of transfer()\n");
    return trc;
}

/* ------------------------------------------------------------- */

static int open_handle (int where, int nsite, int action, char *LF)
{
    int   mode, hnd, i;
    char  *p;

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
    if (where == DOWN && access (LF, F_OK) == 0 && action == ACT_OVERWRITE)
    {
        p = str_strdup1 (LF, 16);
        for (i=1; ; i++)
        {
            sprintf (p, "%s.%d", LF, i);
            if (access (p, F_OK) != 0) break;
        }
        rename (LF, p);
        free (p);
    }

    hnd = open (LF, mode, 0666);
    if (hnd < 0)
    {
        PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_CANT_OPEN), LF);
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

static int issue_transfer_cmd (int where, int nsite, int ns2, int action,
                               char *cmd, char *cmd2, int64_t lsize, int *sock)
{
    int             sock1;
    int             rc, rsp;
    unsigned long   ip, h_ip;
    unsigned int    port, h_po;

    if (is_http_proxy(nsite))
    {
        if (action == ACT_REGET) return ERR_RESTFAIL;
        // connect to proxy
        sock1 = Connect2 (nsite, firewall.fwip, htons (options.fire_port));
        if (sock1 < 0) PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_CANNOT_CONNECT));
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
            rc = swallow_http_headers (nsite, sock1);
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

    if (where == FXP)
    {
        // first we need to issue PASV on recipient to get the port number
        rc = IssuePASVCommand (ns2, &ip, &port);
        switch (rc)
        {
            case ERR_TRANSIENT:
            case ERR_FATAL:
            case ERR_SKIP:
            case ERR_CANCELLED:  return rc;
        }
        // then we tell sender to establish connection
        h_ip = ntohl (ip);
        h_po = ntohs (port);
        rc = SendToServer (nsite, TRUE, &rsp, NULL, "PORT %lu,%lu,%lu,%lu,%u,%u",
                           (h_ip/(256*256*256)) % 256,
                           (h_ip/(256*256))     % 256,
                           (h_ip/256)           % 256,
                           (h_ip)               % 256,
                           (h_po/256)           % 256,
                           (h_po)               % 256);
        if (rsp == 5) return ERR_FATAL; // FXP isn't allowed...
        if (rc || rsp != 2) return ERR_TRANSIENT;
        sock1 = -1;
    }
    else
    {
        // establish data communication channel
        if (status.passive_mode[nsite])
        {
            rc = IssuePASVCommand (nsite, &ip, &port);
            switch (rc)
            {
            case ERR_TRANSIENT:
            case ERR_FATAL:
            case ERR_SKIP:
            case ERR_CANCELLED:  return rc;
            }
            sock1 = Connect2 (nsite, ip, port);
            if (sock1 < 0) PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_CANNOT_CONNECT));
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
            h_ip = ntohl (site[nsite].l_ip);
            h_po = ntohs (port);
            // tell remote where to communicate
            rc = SendToServer (nsite, TRUE, &rsp, NULL, "PORT %lu,%lu,%lu,%lu,%u,%u",
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
                if (port_failed >= 5) status.passive_mode[nsite] = TRUE;
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
    }

    if ((where == DOWN || where == FXP) && action == ACT_REGET)
    {
        // try to do reget if needed
        rc = SendToServer (nsite, FALSE, &rsp, NULL, "REST %s", printf64(lsize));
        if (rc && where != FXP) socket_close (sock1);
        switch (rc)
        {
        case ERR_TRANSIENT:
        case ERR_FATAL:
        case ERR_SKIP:
        case ERR_CANCELLED:  return rc;
        }
        if (rsp != 3)
        {
            PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_REGET_ISNT_SUPPORTED));
            if (where != FXP) socket_close (sock1);
            site[nsite].features.rest_works = FALSE;
            return ERR_RESTFAIL;
        }
    }

    // issue command to start transfer
    rc = SendToServer2 (nsite, FALSE, &rsp, NULL, cmd);
    if (rc && where != FXP) socket_close (sock1);
    switch (rc)
    {
    case ERR_TRANSIENT:
    case ERR_FATAL:
    case ERR_SKIP:
    case ERR_CANCELLED:  return rc;
    }
    if (where == UP && action == ACT_REGET && rsp == 5)
    {
        PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_REGET_ISNT_SUPPORTED));
        if (where != FXP) socket_close (sock1);
        site[nsite].features.appe_works = FALSE;
        return ERR_RESTFAIL;
    }
    if (rsp != 1)
    {
        dmsg ("response is not 1\n");
        if (where != FXP) socket_close (sock1);
        if (rsp == 4 && site[nsite].system_type != SYS_AS400) return ERR_TRANSIENT;
        return ERR_SKIP;
    }

    if (where == FXP)
    {
        rc = SendToServer2 (ns2, FALSE, &rsp, NULL, cmd2);
        switch (rc)
        {
        case ERR_TRANSIENT:
        case ERR_FATAL:
        case ERR_SKIP:
        case ERR_CANCELLED:  return rc;
        }
        if (where == UP && action == ACT_REGET && rsp == 5)
        {
            PutLineIntoResp (RT_COMM, ns2, MSG(M_RESP_REGET_ISNT_SUPPORTED));
            site[ns2].features.appe_works = FALSE;
            return ERR_RESTFAIL;
        }
        if (rsp != 1)
        {
            dmsg ("STOR response is not 1! need to swallow response from nsite\n");
            SendToServer (nsite, FALSE, &rsp, NULL, NULL);
            return ERR_SKIP;
        }
    }
    else
    {
        if (!status.passive_mode[nsite])
        {
            *sock = Accept (nsite, sock1);
            socket_close (sock1);
            if (*sock < 0)
            {
                PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_CONNECTION_NOT_ESTABLISHED));
                return *sock;
            }
        }
        else
        {
            *sock = sock1;
        }
    }

    return ERR_OK;
}

/* ------------------------------------------------------------- */

static int transfer_fxp (int nsite, int ns2, char *LF, char *RF,
                         progress *tot, double *interval_start)
{
    int             trc = -1, key, rc;
    struct timeval  tv;
    fd_set          rdfs;

    dmsg ("entering transfer_fxp\n");
    do
    {
        ShowProgress (nsite, ns2, RF, LF, PROGRESS_FXP, FXP, 0.0, tot, NULL, interval_start);
        tv.tv_sec = 0; tv.tv_usec = 500000;
        FD_ZERO (&rdfs);
        FD_SET (site[nsite].cc_sock, &rdfs);
        FD_SET (site[ns2].cc_sock, &rdfs);
        rc = select (FD_SETSIZE, &rdfs, NULL, NULL, &tv);
        //dmsg ("select() returned %d in fxp\n", rc);
        if (rc >= 2) trc = TR_OK;

        do
        {
            key = getmessage (0);
            if (cmdline.batchmode) continue;
            switch (key)
            {
            case 'q':
            case 'Q':
            case FMSG_BASE_MENU+KEY_STOP_TRANSFER:
                CloseControlConnection (nsite);
                CloseControlConnection (ns2);
                trc = TR_CANCEL;
                break;

            case 's':
            case 'S':
            case FMSG_BASE_MENU+KEY_SKIP_TRANSFER:
                CloseControlConnection (nsite);
                CloseControlConnection (ns2);
                trc = TR_SKIP;
                break;
                /*
            case 'r':
            case 'R':
            case FMSG_BASE_MENU+KEY_RESTART_TRANSFER:
                CloseControlConnection (nsite);
                CloseControlConnection (ns2);
                trc = TR_RETRY;
                break;
                */
            }
        }
        while (trc == -1 && key != 0 && key != -1);
    }
    while (trc == -1);

    return trc;
}

/* ------------------------------------------------------------- */

static int transfer_data (int where, int nsite, int sock, int hnd, char *LF, char *RF,
                          int ind_type, progress *tot, progress *cur,
                          double *interval_start)
{
    int             numbytes = 0, nb = 0, nbs, key, br, rc;
    int             bufsize, lopt, trc;
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
    if (where == DOWN || where == UP)
    {
        lopt = sizeof (bufsize);
        rc = getsockopt (sock, SOL_SOCKET,
                         where == DOWN ? SO_RCVBUF : SO_SNDLOWAT,
                         (void *) &bufsize, &lopt);
        dmsg ("getsockopt(SO_RCV/SNDBUF)=%d; bufsize = %d\n", rc, bufsize);
#endif
    }
    if (rc || bufsize == 0) bufsize = 32*1024;
    // allocate transfer buffer
    bufin = malloc (bufsize);

    dmsg ("transfer: main loop entered\n");
    br = 0;
    trc = -1;
    do
    {
        //dmsg ("transfer: main loop 1\n");
        if (status.transfer_paused)
        {
            tv.tv_sec = 0; tv.tv_usec = 200000;
            select (1, NULL, NULL, NULL, &tv);
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
                if (site[nsite].speed < 100000.)
                {
                    FD_ZERO (&rdfs);
                    FD_SET (sock, &rdfs);
                    rc = select (sock+1, &rdfs, NULL, NULL, &tv);
                    //dmsg ("transfer: select (read) returns %d; errno = %d\n", rc, errno);
                    if (rc < 0 && errno == EINTR) break;
                    if (rc < 0) {trc = TR_OK; break; }
                    if (rc == 0) break;
                }

                nb = recv (sock, bufin, bufsize, 0);
                //dmsg ("transfer: received %d bytes on recv()\n", nb);
                if (nb <= 0) {trc = TR_OK; break; }
                activity = clock1 ();

                numbytes = write (hnd, bufin, nb);
                if (nb != numbytes)
                {
                    if (!is_batchmode)
                        fly_ask_ok (ASK_WARN, MSG(M_DISK_FULL), LF);
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

                //dmsg ("speed = %f\n", site[nsite].speed);
                if (site[nsite].speed < 100000.)
                {
                    FD_ZERO (&wrfs);
                    FD_SET (sock, &wrfs);
                    errno = 0;
                    //dmsg ("entering select(write)... ");
                    rc = select (sock+1, NULL, &wrfs, NULL, &tv);
                    //dmsg ("returned %d; errno = %d\n", rc, errno);
                    if (rc < 0 && errno == EINTR) break;
                    if (rc < 0) {trc = TR_RETRY; break; }
                    if (rc == 0)
                    {
                        lseek (hnd, -nb, SEEK_CUR);
                        br -= nb;
                        break;
                    }
                }
                //dmsg ("read %ld bytes from file\n", br);

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
                    else if (errno != EINTR)
                    {
                        trc = TR_RETRY;
                        break;
                    }
                    goto send_again;
                }

                activity = clock1 ();
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
                    CloseControlConnection (nsite);
                    break;

                case 'p':
                case 'P':
                case FMSG_BASE_MENU+KEY_PAUSE_TRANSFER:
                    if (!status.transfer_paused)  tot->spent += clock1()-*interval_start;
                    else                          *interval_start = clock1();
                    status.transfer_paused = !status.transfer_paused;
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
        ShowProgress (nsite, -1, RF, LF, ind_type, where, now-activity, tot, cur, interval_start);

        //if (now-activity > 10000.) dmsg ("[%f sec]", now-activity);

        //dmsg ("@@@ now-activity=%f, options.data_timeout=%d\n", now-activity, options.data_timeout);
        if (((site[nsite].features.rest_works && now-activity > options.data_timeout) ||
             (!site[nsite].features.rest_works && now-activity > options.data_timeout*5)) &&
            (where == DOWN || where == LIST) &&
            ind_type != PROGRESS_MSG &&
            !status.transfer_paused &&
            !is_http_proxy(nsite)
            )
        {
            dmsg ("data timeout (%f)!\n", now-activity);
            trc = TR_RETRY;
            CloseControlConnection (nsite);
        }
    }
    while (trc == -1);

    dmsg ("transfer: exiting main loop\n");
    ShowProgress (nsite, -1, RF, LF, ind_type, where, 0., tot, cur, interval_start);
    free (bufin);

    if (is_http_proxy(nsite) && where == UP) swallow_http_headers (nsite, sock);
    socket_close (sock);

    return trc;
}

/* ------------------------------------------------------------- */

static int transfer_restart (int where, int nsite, int ns2, trans_item T,
                             char *LF, int64_t *restsize, int nt)
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
            to_mtime = gm2local (st.st_mtime);
        }
    }
    else if (where == UP)
    {
        // search for remote file
        for (j=0; j<site[nsite].ndir; j++)
        {
            if (strcmp (site[nsite].dir[j].name, T.r_dir) == 0) break;
        }
        if (j == site[nsite].ndir)
        {
            dmsg ("upload: remote dir not found: overwrite\n");
            return ACT_OVERWRITE;
        }
        for (k=0; k<site[nsite].dir[j].nfiles; k++)
        {
            if (strcmp (site[nsite].dir[j].files[k].name, T.r_name) == 0) break;
        }
        if (k == site[nsite].dir[j].nfiles)
        {
            dmsg ("upload: remote file not found: overwrite\n");
            return ACT_OVERWRITE;
        }
        to_size = site[nsite].dir[j].files[k].size;
        to_mtime = site[nsite].dir[j].files[k].mtime;
    }
    else if (where == FXP)
    {
        // search for remote file
        for (j=0; j<site[ns2].ndir; j++)
        {
            if (strcmp (site[ns2].dir[j].name, T.l_dir) == 0) break;
        }
        if (j == site[ns2].ndir)
        {
            dmsg ("fxp: remote dir not found: overwrite\n");
            return ACT_OVERWRITE;
        }
        for (k=0; k<site[ns2].dir[j].nfiles; k++)
        {
            if (strcmp (site[ns2].dir[j].files[k].name, T.l_name) == 0) break;
        }
        if (k == site[ns2].dir[j].nfiles)
        {
            dmsg ("fxp: remote file not found: overwrite\n");
            return ACT_OVERWRITE;
        }
        if (options.fxp_skips) return ACT_SKIP;
        to_size = site[ns2].dir[j].files[k].size;
        to_mtime = site[ns2].dir[j].files[k].mtime;
    }
    else
    {
        return ACT_OVERWRITE;
    }

    now = gm2local(time (NULL));
    dmsg ("\ntransfer restart: (now %lu)\n"
          "to_size = %s, from_size = %s; \n"
          "to_time = %lu, from_time = %lu\n",
          now,
          printf64(to_size), printf64(T.size), to_mtime, T.mtime);

    *restsize = to_size;
    decision = ACT_OVERWRITE;

    // SmartAppend
    if (T.mtime > to_mtime && to_mtime < now) decision = ACT_OVERWRITE;
    else
    {
        if (T.size == to_size)     decision = ACT_SKIP;
        else if (T.size > to_size) decision = ACT_REGET;
        else                       decision = ACT_OVERWRITE;
    }

    if (nt <= options.batch_limit && !cmdline.batchmode)
    {
        // ask user
        tm1 = *gmtime (&T.mtime);
        tm2 = *gmtime (&to_mtime);
        strcpy (n1, pretty64 (T.size));
        strcpy (n2, pretty64 (to_size));

        actions[0] = "";
        actions[1] = MSG(M_TR_OVERWRITE);
        actions[2] = MSG(M_TR_RESUME);
        actions[3] = MSG(M_TR_SKIP);
        actions[4] = MSG(M_TR_CANCEL);
        sprintf (answers, " %s \n %s \n %s \n %s ", actions[1], actions[2], actions[3], actions[4]);
        k = fly_ask (0, MSG (M_TR_QUESTION),
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

static int create_dir (int nsite, char *d)
{
    char *p, *p1;
    int  rc, rsp;

    rc = SendToServer (nsite, TRUE, &rsp, NULL, "MKD %s", d);
    if (rc != 0) return -1;

    if (rsp != 2)
    {
        p = strdup (d);
        p1 = strrchr (p, '/');
        if (p1 == NULL) {free (p); return -1;}
        *p1 = '\0';
        rc = create_dir (nsite, p);
        if (rc == 0)
        {
            rc = SendToServer (nsite, TRUE, &rsp, NULL, "MKD %s", d);
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

RC retrieve_dir (int nsite, char *dirname, int dir_no)
{
    int            rc, i, a, prevmode, trc, errcnt;
    char           *buf, *p, tmpfile[MAX_PATH];
    //int          gf_total, gf_trans;
    trans_item     T;

    if (options.history_everything)
    {
        history_add (&site[nsite].u);
    }

    // Getting directory listing
    put_message (MSG(M_RETRIEVING_FILELIST));
    tmpnam1 (tmpfile);
    p = strrchr (tmpfile, '/');
    *p = '\0';

    T.r_dir  = strdup (dirname);
    T.l_dir  = strdup (tmpfile);
    T.r_name = strdup ("");
    T.l_name = strdup (p+1);
    T.mtime  = 0;
    T.size   = 0;
    T.perm   = 0600;
    T.description = NULL;
    T.f = NULL;
    T.flags = TF_OVERWRITE;

    prevmode = status.binary;
    if (site[nsite].system_type == SYS_MVS_NATIVE && !is_http_proxy(nsite) && status.binary == TRUE)
    {
        status.binary = FALSE;
        SetTransferMode (nsite);
    }

    errcnt = 0;
repeat_list:
    trc = transfer (LIST, nsite, -1, &T, 1, FALSE);
    if (trc == ERR_TRANSIENT)
    {
        errcnt++;
        if (errcnt < 10) goto repeat_list;
    }
    rc = 0;

    if (prevmode != status.binary)
    {
        status.binary = prevmode;
        SetTransferMode (nsite);
    }

    // pretend everything OK if LIST failed
    if (rc == ERR_SKIP) rc = 0;

    // Analyzing it
    *p = '/';
    if (load_file (tmpfile, &buf) < 0)
    {
        //return ERR_FATAL;
        buf = strdup (" ");
    }
    remove (tmpfile);

    if (site[nsite].system_type == SYS_WINNTDOS && strstr (buf, "owner") && strstr (buf, "group"))
    {
        site[nsite].system_type = SYS_WINNT;
    }

/*
    site[current_site].system_type = SYS_MVS_NATIVE;

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
    site[current_site].system_type = SYS_MVS_NATIVE;
*/
    if (site[nsite].features.has_mlst)
    {
        analyze_listing (nsite, buf, dir_no, dirname, SYS_MLST);
    }
    else
    {
        analyze_listing (nsite, buf, dir_no, dirname, site[nsite].system_type);
    }
    RN_CURDIR.name = strdup (dirname);
    sort_remote_files (nsite);

    free (buf);

    // look for index files
    if (status.load_descriptions && !is_batchmode)
    {
        for (i=0; i<RN_NF; i++)
            if (isindexfile (RN_FILE(i).name)) break;

        if (i != RN_NF)
        {
            a = 1;
            if (RN_FILE(i).size >= options.desc_size_limit*1024 ||
                (site[nsite].speed > 0. && RN_FILE(i).size/site[nsite].speed > options.desc_time_limit))
            {
                a = fly_ask (0, MSG(M_LARGE_INDEX_FILE),
                             MSG(M_DESC_DOWNLOAD_OPTIONS),
                             RN_FILE(i).name,
                             pretty64(RN_FILE(i).size));
            }
            if (a == 3) status.load_descriptions = FALSE;
            if (a == 1) attach_descriptions (nsite, i);
        }
    }
    set_window_name ("%s - %s", site[nsite].u.hostname, RN_CURDIR.name);
    site[nsite].updated = time (NULL);

    return rc;
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
RC r_chdir (int nsite, char *dirname, int mark)
{
    int     prevdir, i, do_cd, rc;
    char    newdir[2048], *oldpath=NULL, *p;

    do_cd = TRUE;

    if (dirname[0] != '\0') oldpath = strdup (RN_CURDIR.name);

    if (is_http_proxy(nsite))
    {
        // HTTP proxy (can't do "cd")
        if (dirname[0] == '\0') return 0;

        if (dirname[0] == '/' || (strlen (dirname) > 1 && dirname[1] == ':'))
        {
            strcpy (newdir, dirname);
        }
        else
        {
            strcpy (newdir, RN_CURDIR.name);
            str_cats (newdir, dirname);
            sanify_pathname (newdir);
        }
        do_cd = FALSE;
    }
    else
    {
        // not HTTP proxy
        if (site[nsite].features.unixpaths)
        {
            // if dirname is empty, find out what current directory is
            if (dirname[0] == '\0')
            {
                rc = get_dirname (nsite, newdir);
                if (rc) return rc;
                do_cd = FALSE;
            }
            // if ~username is used, try to change to it and then see where we landed
            // or this is first invocation and path isn't absolute
            else if (dirname[0] == '~' || (dirname[0] == '/' && dirname[1] == '~') ||
                     (site[nsite].ndir == 1 && dirname[0] != '/' && (strlen (dirname) > 1 && dirname[1] != ':')))
            {
                if (dirname[0] == '/' && dirname[1] == '~')
                    rc = set_wd (nsite, dirname+1, TRUE);
                else
                    rc = set_wd (nsite, dirname, TRUE);
                //if (rc) return rc;
                rc = get_dirname (nsite, newdir);
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
                strcpy (newdir, RN_CURDIR.name);
                str_cats (newdir, dirname);
            }

            // come and make me holy again
            sanify_pathname (newdir);
        }
        else
        {
            // site[current_site].features.unixpaths == FALSE.
            if (dirname[0] != '\0')
            {
                put_message (MSG(M_CHANGING_DIRECTORY), newdir);
                rc = set_wd (nsite, dirname, TRUE);
                if (site[nsite].ndir != 1 &&
                    (rc == ERR_FATAL || rc == ERR_CANCELLED || rc == ERR_TRANSIENT)) return rc;
            }
            rc = get_dirname (nsite, newdir);
            if (rc) return rc;
            do_cd = FALSE;
        }
    }

    // check whether we have new directory in the cache
    // (searching amongst already read dirs)
    for (i=1; i<site[nsite].ndir; i++)
    {
        if (strcmp (site[nsite].dir[i].name, newdir) == 0)
        {
            site[nsite].cdir = i;
            set_window_name ("%s - %s", site[nsite].u.hostname, RN_CURDIR.name);
            sort_remote_files (nsite);
            if (oldpath != NULL && !strncmp(oldpath, RN_CURDIR.name, strlen (RN_CURDIR.name)))
            {
                p = oldpath + strlen (RN_CURDIR.name);
                if (*p == '/') p++;
                adjust_cursor (&RN_CURDIR, p);
            }
            return 0;
        }
    }

    // try to go to newly guessed directory
    if (do_cd)
    {
        //if (options.autocontrol) display.view_mode = VIEW_CONTROL;
        put_message (MSG(M_CHANGING_DIRECTORY), newdir);
        rc = set_wd (nsite, newdir, TRUE);
        if (rc)
        {
            get_dirname (nsite, newdir);
            return rc;
        }
        if (site[nsite].system_type == SYS_IBMOS2FTPD ||
            site[nsite].system_type == SYS_WINNTDOS ||
            site[nsite].system_type == SYS_OS2NEOLOGIC)
        {
            rc = get_dirname (nsite, newdir);
            if (rc) return rc;
        }
    }

    // check (AGAIN!) whether we have new directory in the cache
    // (searching amongst already read dirs)
    for (i=1; i<site[nsite].ndir; i++)
    {
        if (strcmp (site[nsite].dir[i].name, newdir) == 0)
        {
            site[nsite].cdir = i;
            set_window_name ("%s - %s", site[nsite].u.hostname, RN_CURDIR.name);
            sort_remote_files (nsite);
            if (oldpath != NULL && !strncmp(oldpath, RN_CURDIR.name, strlen (RN_CURDIR.name)))
            {
                p = oldpath + strlen (RN_CURDIR.name);
                if (*p == '/') p++;
                adjust_cursor (&RN_CURDIR, p);
            }
            return 0;
        }
    }

    // new directory, need to obtain listing
    prevdir = site[nsite].cdir;
    //prevdirname = str_sjoin ();
    rc = retrieve_dir (nsite, newdir, -1);
    if (rc)
    {
        site[nsite].cdir = prevdir;
    }
    else
    {
        if (oldpath != NULL && !strncmp(oldpath, RN_CURDIR.name, strlen (RN_CURDIR.name)))
        {
            p = oldpath + strlen (RN_CURDIR.name);
            if (*p == '/') p++;
            adjust_cursor (&RN_CURDIR, p);
        }
        if (mark)
        {
            set_marks (nsite, RN_CURDIR.name);
        }
    }
    //free (prevdirname);
    set_window_name ("%s - %s", site[nsite].u.hostname, RN_CURDIR.name);

    if (oldpath != NULL) free (oldpath);

    display.quicksearch[0] = '\0';
    return rc;
}

/* ------------------------------------------------------------- */

RC set_wd (int nsite, char *newdir, int noisy)
{
    int  rc, rsp;

    if (strcmp (newdir, site[nsite].u.pathname) == 0) return ERR_OK;

    if (is_http_proxy(nsite))
    {
        strcpy (site[nsite].u.pathname, newdir);
        rc = 0;
    }
    else
    {
        // kill terminating slash if present
        //l = strlen (newdir);
        //if (l >= 2 && newdir[l-1] == '/' && newdir[l-2] != ':') newdir[l-1] = '\0';
        if (noisy) put_message (MSG(M_CHANGING_DIRECTORY), newdir);
        rc = SendToServer (nsite, TRUE, &rsp, NULL, "CWD %s", newdir);
        if (rc == ERR_OK && rsp == 2 && site[nsite].features.unixpaths)
        {
            strcpy (site[nsite].u.pathname, newdir);
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
       fly_ask_ok (ASK_WARN, MSG(M_CANT_WRITE_LOGFILE),
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

RC traverse_directory (int nsite, char *dir, int mark)
{
    int             i, d, rc;

    rc = r_chdir (nsite, dir, mark);
    if (rc) return rc;
    d = site[nsite].cdir;

    for (i=0; i<site[nsite].dir[d].nfiles; i++)
        if (site[nsite].dir[d].files[i].flags & FILE_FLAG_DIR
            && strcmp (site[nsite].dir[d].files[i].name, ".."))
        {
            rc = traverse_directory (nsite, site[nsite].dir[d].files[i].name, mark);
            if (getkey (0) == 'q') rc = ERR_CANCELLED;
            if (rc == ERR_FATAL || rc == ERR_CANCELLED) break;
            set_dir_size (nsite, d, i);
        }

    r_chdir (nsite, "..", FALSE);
    return rc;
}

/* ------------------------------------------------------------- */

RC get_dirname (int nsite, char *dirname)
{
    int         rsp, rc, l;
    char        *p1, *p2, separator, buf[1024];

    put_message (MSG(M_QUERYING_DIR));

    rc = SendToServer (nsite, TRUE, &rsp, buf, "PWD");
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
        if (site[nsite].system_type == SYS_VMS_MADGOAT)
        {
            p1 = strstr (dirname, "/000000/");
            if (p1 != NULL) strcpy (dirname, p1+7);
        }
        if (site[nsite].system_type == SYS_VMS_UCX)
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
        if (site[nsite].system_type == SYS_MVS_NATIVE)
        {
            str_strip2 (dirname, "'.");
        }
        dmsg ("get_dirname: returning [%s]\n", dirname);
        // never ever return backslashes!
        str_translate (dirname, '\\', '/');
        // drive letters are always case insensitive
        if (strlen(dirname) > 1 && dirname[1] == ':') dirname[0] = tolower (dirname[0]);
        // kill terminating slash if present
        l = strlen (dirname);
        if (l >= 2 && dirname[l-1] == '/' && dirname[l-2] != ':') dirname[l-1] = '\0';
        strcpy (site[nsite].u.pathname, dirname);
        return ERR_OK;
    }
    else
    {
        if (!is_batchmode)
            fly_ask_ok (ASK_WARN, MSG(M_CANT_GUESS_CURDIR));
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

static RC IssuePASVCommand (int nsite, unsigned long *ip, int *port)
{
    int             i, rc, rsp;
    char            *p, *p1, resp[64];
    unsigned char   h[6];
    u_short         port1;

    rc = SendToServer (nsite, TRUE, &rsp, resp, "PASV");
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

static void ShowProgress (int nsite, int ns2,
                          char *rfilename, char *lfilename, int ind_type,
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

    if (ind_type == PROGRESS_FXP)
    {
        ShowProgressFXP (nsite, ns2, stall, rfilename, lfilename,
                         tot, interval_start);
        return;
    }

    updlimit = 0.3;
    if (site[nsite].speed > 10.) updlimit = 0.5;
    if (fl_opt.is_unix) updlimit = 1.0;
    if (site[nsite].speed > 100.) updlimit = 1.2;
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
    if (tot_spent > 0.) site[nsite].speed = (double)(tot->tr.bytes)/tot_spent;

    cur_eta = site[nsite].speed == 0. ? 0. : (unsigned long)(cur_re.bytes/site[nsite].speed);
    tot_eta = site[nsite].speed == 0. ? 0. : (unsigned long)(tot_re.bytes/site[nsite].speed);

    // change session title to reflect transfer progress
    is_download = where == DOWN || where == LIST || where == RETATTRS || where == LSATTRS;
    if (now-prev2 > 2. || percent >= 99)
    {
        if (stall > options.stalled_interval)
        {
            set_window_name ("%s %s %u%% %s %s",
                             is_download ? "->" : "<-",
                             fl_opt.is_unix ? "stalled" : MSG(M_STALLED),
                             percent,
                             is_download ? "from" : "to",
                             site[nsite].u.hostname);
            stalled_window_name = TRUE;
        }
        else
        {
            set_window_name ("%s %u%% at %.2f KB/s %s %s",
                             is_download ? "->" : "<-",
                             percent, site[nsite].speed/1000.,
                             is_download ? "from" : "to",
                             site[nsite].u.hostname);
            stalled_window_name = FALSE;
        }
        prev2 = now;
        prev_percent = percent;
    }

    if (stall < options.stalled_interval && stalled_window_name)
    {
            set_window_name ("%s %u%% at %.2f KB/s %s %s",
                             is_download ? "->" : "<-",
                             percent, site[nsite].speed/1000.,
                             is_download ? "from" : "to",
                             site[nsite].u.hostname);
            stalled_window_name = FALSE;
    }
    // check for too often updates
    if (now - prev3 < updlimit && cur_re.bytes != 0 && tot_re.bytes != 0 && stall > -0.5) return;
    prev3 = now;

    // message-type progress indicator
    if (ind_type == PROGRESS_MSG)
    {
        if (tot->to.bytes == 0)
            //put_message (MSG(M_BYTES_RECEIVED), tot->tr.bytes);
            put_message ("     %s   \n"
                         "     %s bytes received\n"
                         "     press ESC to stop    ",
                         site[nsite].u.hostname,
                         pretty64(tot->tr.bytes));
        else
            //put_message (MSG(M_BYTES_REMAIN), tot->tr.bytes,
            //             tot->to.bytes-tot->tr.bytes-tot->sk.bytes);
            put_message ("     %s     \n"
                         "     %s bytes received\n"
                         "     %s bytes remain\n"
                         "     press ESC to stop    ",
                         site[nsite].u.hostname,
                         pretty64(tot->tr.bytes),
                         pretty64(tot->to.bytes-tot->tr.bytes-tot->sk.bytes));
        return;
    }

    // format sizes and times
    strcpy (s_stall,     make_time((unsigned long)(stall < 0. ? 0. : stall)));
    strcpy (s_speed,     pretty ((unsigned long)site[nsite].speed));
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
            msg = MSG(M_TRANSFER_COMPLETED);
        else if (stall == -2.0)
            msg = MSG(M_COMMUNICATING);
        else
            msg = status.transfer_paused ? MSG(M_PAUSED) : MSG(M_SQRP);
    }

    if (tot->to.files == 1)
    {
        sprintf (line1, MSG(M_PERCENTAGE1), percent,
                 status.binary ? MSG(M_BINARY) : MSG(M_ASCII),
                 is_download ? MSG(M_DOWNLOAD) : MSG(M_UPLOAD));
    }
    else
    {
        sprintf (line1, MSG(M_PERCENTAGE2), percent,
                 status.binary ? MSG(M_BINARY) : MSG(M_ASCII),
                 is_download ? MSG(M_DOWNLOAD) : MSG(M_UPLOAD),
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
             s_speed, MSG(M_BYTESSEC),
             s_stall, MSG(M_INACT),
             rfilename, lfilename,
             MSG(M_TOTAL),       slack, s_bt_total, s_tt_total,
             MSG(M_TRANSFERRED), slack, s_bt_trans, s_tt_spent,
             MSG(M_SKIPPED),     slack, s_bt_skip,
             MSG(M_LEFT),        slack, s_bt_left, s_tt_left,
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
             s_speed, MSG(M_BYTESSEC),
             s_stall, MSG(M_INACT),
             MSG(M_TOTAL),       slack, s_bt_total, s_tt_total,
             MSG(M_TRANSFERRED), slack, s_bt_trans, s_tt_spent,
             MSG(M_SKIPPED),     slack, s_bt_skip,
             MSG(M_LEFT),        slack, s_bt_left, s_tt_left,
             bar1,
             rfilename, lfilename,
             MSG(M_TOTAL),       slack, s_bc_total, s_tc_total,
             MSG(M_TRANSFERRED), slack, s_bc_trans, s_tc_spent,
             MSG(M_SKIPPED),     slack, s_bc_skip,
             MSG(M_LEFT),        slack, s_bc_left, s_tc_left,
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

static void ShowProgressFXP (int nsite, int ns2, double stall,
                             char *rfilename, char *lfilename,
                             progress *tot, double *interval_start)
{
    static int           prev_percent = 0;
    static double        prev2 = 0., prev3 = 0.;

    double          now, updlimit, tot_eta, interval;
    double          tot_spent;
    progress_data   tot_re;

    int             l1, bar_len, percent;
    char            s_speed[28], bar1[256], slack[256], *msg;
    char            line1[100];
    char            s_bt_total[28], s_bt_trans[28], s_bt_skip[28], s_bt_left[28];
    char            s_tt_total[28], s_tt_spent[28], s_tt_left[28];

    updlimit = 0.3;
    if (site[nsite].speed > 10.) updlimit = 0.5;
    if (fl_opt.is_unix) updlimit = 1.0;
    if (site[nsite].speed > 100.) updlimit = 1.2;
    if (options.slowlink) updlimit = 3.0;

    now = clock1 ();
    interval = now - *interval_start;
    tot_spent = tot->spent + interval;

    // compute numbers
    tot_re.files = max1 (0, tot->to.files - tot->tr.files - tot->sk.files);
    tot_re.bytes = max1 (0, tot->to.bytes - tot->tr.bytes - tot->sk.bytes);
    percent = tot->to.bytes ?
        (int)(((double)tot->tr.bytes+(double)tot->sk.bytes)*100./(double)tot->to.bytes) : 0;
    if (percent > 100.0) percent = 100.0;

    // speed is measured in B/sec
    //if (tot_spent > 0.) site[nsite].speed = (double)(tot->tr.bytes)/tot_spent;

    tot_eta = site[nsite].speed == 0. ? 0. : (unsigned long)(tot_re.bytes/site[nsite].speed);

    // change session title to reflect transfer progress
    if (now-prev2 > 2. || percent >= 99)
    {
        set_window_name ("<-> %u%% at %.2f KB/s %s %s and %s",
                         percent, site[nsite].speed/1000.,
                         "between",
                         site[nsite].u.hostname, site[ns2].u.hostname);
        prev2 = now;
        prev_percent = percent;
    }

    // check for too often updates
    if (now - prev3 < updlimit && tot_re.bytes != 0 && stall > -0.5) return;
    prev3 = now;

    // format sizes and times
    strcpy (s_speed,     pretty ((unsigned long)site[nsite].speed));
    strcpy (s_bt_total,  pretty64 (tot->to.bytes));
    strcpy (s_bt_trans,  pretty64 (tot->tr.bytes));
    strcpy (s_bt_skip,   pretty64 (tot->sk.bytes));
    strcpy (s_bt_left,   pretty64 (tot_re.bytes));
    strcpy (s_tt_total,  make_time ((unsigned long)(tot_spent+tot_eta)));
    strcpy (s_tt_spent,  make_time ((unsigned long)tot_spent));
    strcpy (s_tt_left,   make_time ((unsigned long)tot_eta));

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
    memset (bar1, fl_sym.fill4, l1);
    memset (bar1+l1, fl_sym.fill1, bar_len-l1);
    bar1[bar_len] = '\0';
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
            msg = MSG(M_TRANSFER_COMPLETED);
        else if (stall == -2.0)
            msg = MSG(M_COMMUNICATING);
        else
            //msg = MSG(M_SQRP);
            msg = "S/Q - Skip/Quit";
    }

    sprintf (line1, MSG(M_PERCENTAGE2), percent,
             status.binary ? MSG(M_BINARY) : MSG(M_ASCII),
             "site-to-site", tot->tr.files, tot->to.files);

    put_message
        (" %s \n"
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
         s_speed, MSG(M_BYTESSEC),
         rfilename, lfilename,
         MSG(M_TOTAL),       slack, s_bt_total, s_tt_total,
         MSG(M_TRANSFERRED), slack, s_bt_trans, s_tt_spent,
         MSG(M_SKIPPED),     slack, s_bt_skip,
         MSG(M_LEFT),        slack, s_bt_left, s_tt_left,
         bar1,
         msg
        );
}

/* ------------------------------------------------------------- */

RC cache_file (int nsite, int nd, int nf)
{
    char           tmpfile[MAX_PATH], *p;
    long           lfile;
    int            rc1, rc2, prevmode;
    trans_item     T;
    struct stat    st;

    dmsg ("caching file %s\n", site[nsite].dir[nd].files[nf].name);

    // form the name of the local file
    tmpnam1 (tmpfile);
    p = strrchr (tmpfile, '/');
    *p = '\0';

    T.r_dir  = strdup (site[nsite].dir[nd].name);
    T.l_dir  = strdup (tmpfile);
    T.r_name = strdup (site[nsite].dir[nd].files[nf].name);
    T.l_name = strdup (p+1);
    T.mtime = site[nsite].dir[nd].files[nf].mtime;
    T.size   = site[nsite].dir[nd].files[nf].size;
    T.perm   = 0600;
    T.description = NULL;
    T.f = &(site[nsite].dir[nd].files[nf]);
    T.flags = 0;

    prevmode = status.binary;
    if (site[nsite].system_type == SYS_MVS_NATIVE && !is_http_proxy(nsite) && status.binary == TRUE)
    {
        status.binary = FALSE;
        SetTransferMode (nsite);
    }

    rc1 = transfer (DOWN, nsite, -1, &T, 1, FALSE);
    *p = '/';

    if (prevmode != status.binary)
    {
        status.binary = prevmode;
        SetTransferMode (nsite);
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

    site[nsite].dir[nd].files[nf].size = st.st_size;

    lfile = load_file (tmpfile, &site[nsite].dir[nd].files[nf].cached);
    remove (tmpfile);

    return ERR_OK;
}

/* ------------------------------------------------------------- */

RC attach_descriptions (int nsite, int nf)
{
    int    rc, i;
    char   *d;

    // load descriptions into file
    if (RN_FILE(nf).cached == NULL)
    {
        rc = cache_file (nsite, site[nsite].cdir, nf);
        if (rc) return rc;
    }

    // place description
    str_parseindex (RN_FILE(nf).cached);
    for (i=0; i<RN_NF; i++)
    {
        d = str_finddesc (RN_FILE(i).name, RN_FILE(i).size);
        //d = str_find_description (RFILE(nf).cached, RFILE(i).name, RFILE(i).size);
        if (d != NULL)
        {
            RN_FILE(i).desc = strdup (d);
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
    int    i, rc, nsite;
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
    nsite = 0;
    rc = Login (nsite, &u, V_RIGHT);
    if (rc) return ERR_SKIP;

    // look for specified file
    for (i=0; i<RN_NF; i++)
    {
        if (strcmp (RN_FILE(i).name, file) == 0) break;
    }

    T.r_dir = strdup (RN_CURDIR.name);
    T.l_dir = strdup (local[V_LEFT].dir.name);
    T.r_name = strdup (file);
    T.l_name = strdup (file);
    T.mtime = i == RN_NF ? time(NULL) : RN_FILE(i).mtime;
    T.size   = i == RN_NF ? 0 : RN_FILE(i).size;
    T.perm   = i == RN_NF ? 0644 : RN_FILE(i).rights;
    T.description = i == RN_NF ? NULL : RN_FILE(i).desc;
    T.f = i == RN_NF ? NULL : &(RN_FILE(i));
    T.flags = 0;
    if (options.lowercase_names &&
        str_lastspn (T.l_name, "abcdefghijklmnopqrstuvwxyz") == NULL)
        str_lower (T.l_name);

    rc = transfer (DOWN, nsite, -1, &T, 1, FALSE);

    return rc;
}

/* -------------------------------------------------------------
 puts file given by url from the current directory
 returns: ERR_OK/ERR_FATAL/ERR_SKIP/ERR_CANCELLED
 */

int do_put (char *url)
{
    url_t  u;
    char   *file, path[2048];
    int    i, rc, nsite;
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
    nsite = 0;
    rc = Login (nsite, &u, V_RIGHT);
    if (rc) return ERR_SKIP;

    // look for local file
    l_chdir (V_LEFT, NULL);
    for (i=0; i<LNFILES(V_LEFT); i++)
    {
        if (strcmp (LFILE(V_LEFT, i).name, file) == 0) break;
    }
    if (i == LNFILES(V_LEFT)) return ERR_SKIP;

    T.r_dir  = strdup (RN_CURDIR.name);
    T.l_dir  = strdup (local[V_LEFT].dir.name);
    T.r_name = strdup (LFILE(V_LEFT, i).name);
    T.l_name = strdup (LFILE(V_LEFT, i).name);
    T.mtime  = LFILE(V_LEFT, i).mtime;
    T.size   = LFILE(V_LEFT, i).size;
    T.perm   = LFILE(V_LEFT, i).rights;
    T.description = LFILE(V_LEFT, i).desc;

    rc = transfer (UP, nsite, -1, &T, 1, FALSE);

    return rc;
}

/* ------------------------------------------------------------- */

static int swallow_http_headers (int nsite, int sock)
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
            PutLineIntoResp (RT_RESP, nsite, "%s", header);
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
                    if (entryfield (MSG(M_ENTER_FIREWALL_USERID), firewall.userid,
                                    sizeof (firewall.userid), 0) == PRESSED_ESC ||
                        entryfield (MSG(M_ENTER_FIREWALL_PASSWORD), firewall.password,
                                    sizeof (firewall.password), LI_INVISIBLE) == PRESSED_ESC)
                    {
                        cancelled = TRUE;
                    }
                }
                authenticate = TRUE;
            }
            if (strstr (p1, "squid") != NULL)
            {
                site[nsite].system_type = SYS_SQUID;
            }
            else if (strstr (p1, "netscape-proxy") != NULL)
            {
                site[nsite].system_type = SYS_NSPROXY;
            }
            else if (strstr (p1, "bordermanager") != NULL)
            {
                site[nsite].system_type = SYS_BORDERMANAGER;
            }
            else if (strstr (p1, "apache") != NULL)
            {
                site[nsite].system_type = SYS_APACHE;
            }
            else if (strstr (p1, "inktomi") != NULL)
            {
                site[nsite].system_type = SYS_INKTOMI;
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

static void transfer_bfs_attrs (int where, int nsite, trans_item *t)
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
        if (site[nsite].features.has_bfs1 == TRUE)
        {
            // temporary file to hold the attribute list
            tmpnam1 (tmpfile);
            p = strrchr (tmpfile, '/');
            *p = '\0';
            T.l_dir   = strdup (tmpfile);
            T.l_name  = strdup (p+1);
            T.r_dir   = strdup (T.r_dir);
            T.r_name  = strdup (T.r_name);
            // download the attribute list
            rc = transfer (LSATTRS, nsite, -1, &T, 1, FALSE);
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
                sprintf (buffer, "%s %s", t->r_name, attrs[i].name);
                T.r_name = strdup (buffer);
                T.l_dir  = strdup (tmpfile);
                T.l_name = strdup (p+1);
                T.r_dir  = strdup (T.r_dir);
                rc = transfer (RETATTRS, nsite, -1, &T, 1, FALSE);
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
            nd = find_directory (nsite, T.r_dir);
            dmsg ("found directory %d\n", nd);
            if (nd == -1) return; // not found. hmm....
            // search target
            p = str_join (T.r_name, ATTR_SUFFIX);
            for (i=0; i<site[nsite].dir[nd].nfiles; i++)
            {
                if (strcmp (site[nsite].dir[nd].files[i].name, p) == 0) break;
            }
            free (p);
            if (i == site[nsite].dir[nd].nfiles) return;
            // download that file
            tmpnam1 (tmpfile);
            p = strrchr (tmpfile, '/');
            *p = '\0';
            T.r_dir  = strdup (site[nsite].dir[nd].name);
            T.l_dir  = strdup (tmpfile);
            T.r_name = strdup (site[nsite].dir[nd].files[i].name);
            T.l_name = strdup (p+1);
            // download the attribute list
            rc = transfer (DOWN, nsite, -1, &T, 1, FALSE);
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
        dmsg ("fetched %d attributes; has_bfs1=%d\n", na, site[nsite].features.has_bfs1);
        if (na <= 0) return; // nothing to worry about....

        if (site[nsite].features.has_bfs1 == TRUE)
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
                sprintf (buffer, "%s %s %x", t->r_name, attrs[i].name, attrs[i].type);
                T.r_name = strdup (buffer);
                T.r_dir  = strdup (T.r_dir);
                T.l_name = strdup (T.l_name);
                T.l_dir  = strdup (T.l_dir);
                T.size = attrs[i].length;
                dmsg ("calling SETATTRS");
                transfer (SETATTRS, nsite, -1, &T, 1, FALSE);
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
            T.r_dir  = strdup (T.r_dir);
            T.l_dir  = strdup (tmpfile);
            T.l_name = strdup (p+1);
            T.size = st.st_size;
            // upload the attribute file
            rc = transfer (UP, nsite, -1, &T, 1, FALSE);
            *p = '/';
            remove (tmpfile);
        }
        destroy_attrs (attrs, na);
    }
}

/* compose command to initiate transfer */
static char *compose_command (int where, int nsite, trans_item T, int action)
{
    char *cmd, *cmdprefix, *p, h1[128], buf[128], flags[16];

    cmd = malloc (2048);

    if (is_http_proxy(nsite))
    {
        // 1. GET/PUT request
        if (where == DOWN || where == LIST) cmdprefix = "GET ";
        else if (where == UP)               cmdprefix = "PUT ";
        else                                return NULL;
        strcpy (cmd, cmdprefix);
        p = compose_url (site[nsite].u, T.r_name, TRUE, site[nsite].system_type != SYS_NSPROXY);
        if (p == NULL) return NULL;
        strcat (cmd, p);
        free (p);
        strcat (cmd, " HTTP/1.0\n");
        p = compose_url (site[nsite].u, T.r_name, FALSE, site[nsite].system_type != SYS_NSPROXY);
        PutLineIntoResp (RT_CMD, nsite, "%s%s HTTP/1.0", cmdprefix, p);
        free (p);
        // 2. Host:
        strcat (cmd,  "Host: ");
        strcat (cmd, site[nsite].u.hostname);
        strcat (cmd, "\n");
        PutLineIntoResp (RT_CMD, nsite, "Host: %s", site[nsite].u.hostname);
        // 3. Authentication (if needed)
        if (http_authenticate)
        {
            sprintf (buf, "%s:%s", firewall.userid, firewall.password);
            p = base64_encode (buf, -1);
            strcat (cmd, "Proxy-Authorization: Basic ");
            strcat (cmd, p);
            strcat (cmd, "\n");
            PutLineIntoResp (RT_CMD, nsite, "Proxy-Authorization: Basic *");
            free (p);
        }
        // 4. Content-type for upload
        if (where == UP)
        {
            strcat (cmd, "Content-Type: application/octet-stream\n");
            PutLineIntoResp (RT_CMD, nsite, "Content-Type: application/octet-stream");
        }
        // 5. Content-Length for upload
        if (where == UP)
        {
            sprintf (h1, "Content-Length: %s", printf64(T.size));
            strcat (cmd, h1);
            strcat (cmd, "\n");
            PutLineIntoResp (RT_CMD, nsite, h1);
        }
        // 6. User-Agent
        sprintf (h1, "User-Agent: %s", options.user_agent);
        strcat (cmd, h1);
        strcat (cmd, "\n");
        PutLineIntoResp (RT_CMD, nsite, h1);
        // 7. Cache-control when specified
        if (options.flush_http_cache)
        {
            strcat (cmd, "Cache-control: no-cache\n");
            PutLineIntoResp (RT_CMD, nsite, "Cache-control: no-cache");
            strcat (cmd, "Pragma: no-cache\n");
            PutLineIntoResp (RT_CMD, nsite, "Pragma: no-cache");
        }
        // 8. Disable keepalives
        strcat (cmd, "Connection: close\n");
        PutLineIntoResp (RT_CMD, nsite, "Connection: close");
        // 9. Terminate request
        strcat (cmd, "\n");
    }
    else
    {
        switch (where)
        {
        case LIST:
            if (site[nsite].features.has_mlst)
            {
                strcpy (cmd, "MLSD");
            }
            else
            {
                strcpy (cmd, "LIST");
                if (site[nsite].system_type == SYS_UNIX && status.use_flags)
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
        case FXP:
            strcpy (cmd, "RETR ");
            strcat (cmd, T.r_name);
            break;

        case UP:
            strcpy (cmd, "STOR ");
            if (site[nsite].system_type == SYS_AS400 && options.as400_put) strcpy (cmd, "PUT ");
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

/* write/upload file properties */
static void set_properties (int where, int nsite, int ns2, trans_item T, char *LF)
{
    int             rc, rc1, rsp;
    struct          utimbuf ut;
    struct tm       tm1;
    time_t          lt;

    /* we only need to do that for file transfers, not attribute
     transfers or listing transfers */
    if (where != DOWN && where != UP && where != FXP) return;

    if (where == DOWN && options.descfile != NULL && T.description != NULL)
        write_description (T.l_name, T.description, options.descfile);

    // set timestamp
    if (where == DOWN && options.preserve_timestamp_download)
    {
        if (is_time1980 && T.mtime < T_01_01_1980)
        {
            ut.actime  = local2gm (T_01_01_1980);
        }
        else
        {
            ut.actime  = local2gm (T.mtime);
        }
        ut.modtime = ut.actime;
        utime (LF, &ut);
    }

    // SITE UTIME file access-time mod-time
    if (where == UP && options.preserve_timestamp_upload &&
        site[nsite].features.utime_works && !is_http_proxy(nsite))
    {
        // 19901231095959
        lt = local2gm (T.mtime);
        tm1 = *gmtime (&lt);
        SendToServer (nsite, TRUE, &rsp, NULL,
                      "SITE UTIME %s %04u%02u%02u%02u%02u%02u %04u%02u%02u%02u%02u%02u %04u%02u%02u%02u%02u%02u UTC",
                      T.r_name,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
        if (rsp != 2) site[nsite].features.utime_works = FALSE;
    }

    // SITE UTIME file access-time mod-time
    if (where == FXP && options.preserve_timestamp_upload &&
        site[ns2].features.utime_works && !is_http_proxy(ns2))
    {
        // 19901231095959
        lt = local2gm (T.mtime);
        tm1 = *gmtime (&lt);
        SendToServer (ns2, TRUE, &rsp, NULL,
                      "SITE UTIME %s %04u%02u%02u%02u%02u%02u %04u%02u%02u%02u%02u%02u %04u%02u%02u%02u%02u%02u UTC",
                      T.r_name,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
                      tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
                      tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
        if (rsp != 2) site[ns2].features.utime_works = FALSE;
    }

    if (where == DOWN && fl_opt.is_unix && options.preserve_permissions_download)
    {
        rc1 = chmod (LF, T.perm);
    }

    if (where == UP &&
        options.preserve_permissions_upload &&
        site[nsite].system_type == SYS_UNIX &&
        !is_anonymous(nsite) &&
        site[nsite].features.chmod_works &&
        (T.perm & 0777) != 0644 &&
        fl_opt.is_unix &&
        !is_http_proxy(nsite))
    {
        rc = SendToServer (nsite, TRUE, &rsp, NULL, "SITE CHMOD %o %s", T.perm & 0777, T.r_name);
        if (rc == 0 && rsp == 5) site[nsite].features.chmod_works = FALSE;
    }

    if (where == FXP &&
        options.preserve_permissions_upload &&
        site[ns2].system_type == SYS_UNIX &&
        !is_anonymous(ns2) &&
        site[ns2].features.chmod_works &&
        (T.perm & 0777) != 0644 &&
        fl_opt.is_unix &&
        !is_http_proxy(ns2))
    {
        rc = SendToServer (ns2, TRUE, &rsp, NULL, "SITE CHMOD %o %s", T.perm & 0777, T.r_name);
        if (rc == 0 && rsp == 5) site[ns2].features.chmod_works = FALSE;
    }

    dmsg ("deciding whether to transfer BFS attributes:\n");
    dmsg ("where = %d, l_name=%s, r_name=%s\n", where, T.l_name, T.r_name);
    dmsg ("site[nsite].features.has_bfs1=%d, options.use_bfs_files=%d\n",
          site[nsite].features.has_bfs1, options.use_bfs_files);
    if ((where == DOWN || where == UP) &&
        str_tailcmp (T.l_name, ATTR_SUFFIX) &&
        str_tailcmp (T.r_name, ATTR_SUFFIX) &&
        !is_http_proxy(nsite) &&
        (site[nsite].features.has_bfs1 == TRUE || options.use_bfs_files))
    {
        transfer_bfs_attrs (where, nsite, &T);
    }
}

