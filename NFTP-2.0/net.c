#include <fly/fly.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#include <scl.h>

#include "nftp.h"
#include "net.h"
#include "history.h"
#include "auxprogs.h"
#include "network.h"
#include "passwords.h"
#include "do_key.h"

struct _site             site[MAX_SITE];
struct _status           status;
//struct _cchistory        cchistory[MAX_SITE];
struct _firewall         firewall;

static RC   OpenControlConnection (int nsite);
static int  ReadControlConnection (int nsite, char *resp);
static RC   WriteControlConnection (int nsite, char *command);
void        CloseControlConnection (int nsite);
static RC   Setup_site (int nsite, url_t *u);
static int  reconnect (int nsite);

/* -------------------------------------------------------------
 -- Login
 complete login procedure, from setting up 'site' structure to
 retrieving directory listing
 -- parameters
 host   - hostname
 userid - user id
 passwd - password. May be NULL - then user will be prompted
 port   - port to use
 returns: RC
 */

int Login (int nsite, url_t *u, int pnl)
{
    int          rc, pause, i, nconn = MAX_SITE;
    char         buf[64], *p;
    double       target;

    // check for empty hostname
    if (u->hostname[0] == '\0') return ERR_CANCELLED;

    put_message (MSG(M_CONNECTING), u->hostname);

    history_add (u);

    // handle anonymous logins
    if (u->userid[0] == '\0') strcpy (u->userid, options.anon_name);

    // check for password
    if ((strcmp (u->userid, options.anon_name) == 0 ||
         strcmp (u->userid, "ftp") == 0) &&
       u->password[0] == '\0')
    {
        strcpy (u->password, options.anon_pass);
    }
    else if (u->password[0] == '\0')
    {
        p = psw_match (u->hostname, u->userid);
        if (p == NULL)
        {
            if (entryfield (MSG(M_ENTER_PASSWORD), u->password, sizeof(u->password),
                            LI_INVISIBLE) == PRESSED_ESC)
                return ERR_CANCELLED;
        }
        else
        {
            str_scopy (u->password, p);
        }
    }

    // see if we need to look for empty slot
    if (nsite == -1)
    {
        for (i=0; i<MAX_SITE; i++)
        {
            if (!site[i].set_up) break;
        }
        if (i == MAX_SITE)
        {
            if (MAX_SITE == 1)
            {
                if (ask_logoff (0)) return ERR_CANCELLED;
                nsite = 0;
            }
            else
            {
                /* ask user what FTP connection to scrap */
                nsite = fly_choose ("What connection to close?", 0, &nconn, 0, drawline_conn, NULL);
            }
            if (nsite == -1) return ERR_CANCELLED;
        }
        else
        {
            nsite = i;
        }
    }

    // check: previous attempt failed due to wrong password?
    if (site[nsite].set_up &&
        strcmp (u->hostname, site[nsite].u.hostname) == 0 &&
        u->port == site[nsite].u.port &&
        strcmp (u->userid, site[nsite].u.userid) == 0 &&
        strcmp (u->password, site[nsite].u.password) != 0)
        strcpy (site[nsite].u.password, u->password);

    // check: already connected to the same site?
    //if (!site[nsite].set_up ||
    //    strcmp (u->hostname, site[nsite].u.hostname) != 0 ||
    //    u->port != site[nsite].u.port ||
    //    strcmp (u->userid, site[nsite].u.userid) != 0)
    {
        //if (ask_logoff()) return ERR_CANCELLED;

        if (site[nsite].connected) CloseControlConnection (nsite);

        rc = Setup_site (nsite, u);
        if (rc) return rc;

        if (!status.use_proxy[nsite] || options.firewall_type != 5)
        {
            rc = OpenControlConnection (nsite);
            if (rc)
            {
                set_window_name (MSG(M_NOT_CONNECTED));
                site[nsite].set_up = FALSE;
                return rc;
            }
        }
        else
        {
            //site[nsite].system_type = SYS_SQUID;
            site[nsite].system_type = SYS_APACHE;
        }
    }

    if (site[nsite].system_type == SYS_VMS_MULTINET && u->pathname[0] == '\0')
        strcpy (u->pathname, "/");

    if (is_http_proxy(nsite) && u->pathname[0] == '\0')
        strcpy (u->pathname, "/");

    rc = r_chdir (nsite, u->pathname, FALSE) ? r_chdir (nsite, "", FALSE) : 0;
    if (rc)
    {
        CloseControlConnection (nsite);
        return rc;
    }

    display.view[pnl] = nsite;
    return ERR_OK;
}

/* -----------------------------------------------------------------

 -- Logoff
 sends QUIT to site and then closes control connection
 -- parameters:
 none
 -- returns: error code
 0 - success

 */

int Logoff (int nsite)
{
    if (!site[nsite].set_up) return ERR_OK;

    history_add (&site[nsite].u);
    CloseControlConnection (nsite);

    return ERR_OK;
}

/* --------------------------------------------------------------------
 -- Setup_site:
 initializes 'site' structure. sets defaults, resolves name
 -- returns: RC */

static RC Setup_site (int nsite, url_t *u)
{
    struct hostent     *remote;
    struct in_addr     in_addr1;
    unsigned long      addr;

    dmsg ("Setup_site entered; hostname = [%s]\n", u->hostname);

    site[nsite].u = *u;

    // fill in defaults
    site[nsite].connected     = 0;
    site[nsite].cc_sock       = -1; /* for now */
    site[nsite].updated       = -1;
    //site[nsite].batch_mode    = FALSE;
    site[nsite].system_type   = SYS_UNKNOWN;
    //site[nsite].use_PASV      = options.PASV_always;
    site[nsite].sortmode      = abs (options.defaultsort);
    site[nsite].sortdirection = (options.defaultsort >= 0) ? 1 : -1;
    site[nsite].speed         = 0.; /* isn't known yet */

    if (site[nsite].u.flags & URL_FLAG_PASSIVE)
    {
        status.passive_mode[nsite] = TRUE;
    }
    site[nsite].date_fmt = options.as400_date_fmt;

    site[nsite].features.rest_works    = TRUE;
    site[nsite].features.appe_works    = TRUE;
    site[nsite].features.chmod_works   = TRUE;
    site[nsite].features.utime_works   = TRUE;
    site[nsite].features.chall_works   = -1;
    site[nsite].features.has_feat      = FALSE;
    site[nsite].features.has_mdtm      = FALSE;
    site[nsite].features.has_size      = FALSE;
    site[nsite].features.has_rest      = FALSE;
    site[nsite].features.has_tvfs      = FALSE;
    site[nsite].features.has_mlst      = FALSE;
    site[nsite].features.unixpaths     = TRUE;
    site[nsite].features.has_bfs1      = -1;

    // fix some things
    site[nsite].u.pathname[0] = '\0';
    if (site[nsite].u.port == 0) site[nsite].u.port = options.defaultport;

    // clean up userid and hostname. password might contain spaces!
    str_strip2 (site[nsite].u.hostname, "\n\r\t ");
    str_strip2 (site[nsite].u.userid, "\n\r\t ");

    site[nsite].maxndir = 1024;
    site[nsite].dir = malloc (sizeof(directory)*site[nsite].maxndir);

    // simulate empty directory to prevent uninitialized fields
    site[nsite].ndir          = 1;
    site[nsite].cdir          = 0;
    site[nsite].dir[site[nsite].cdir].files  = malloc (sizeof(file) * (1));
    site[nsite].dir[site[nsite].cdir].name   = "";
    site[nsite].dir[site[nsite].cdir].nfiles = 0;
    site[nsite].dir[site[nsite].cdir].first = 0;
    site[nsite].dir[site[nsite].cdir].current = 0;

    // we're all set. now trying to verify hostnames */

    if (firewall.userid[0] == '\0')
        strcpy (firewall.userid, options.fire_login);

    if (firewall.password[0] == '\0')
        strcpy (firewall.password, options.fire_passwd);

    if (is_http_proxy(nsite))
    {
        site[nsite].set_up = 1;
        return ERR_OK;
    }

    PutLineIntoResp (RT_COMM, nsite, "");
    if (!is_firewalled(nsite) && site[nsite].u.ip == 0L)
    {
        PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_LOOKING_UP), site[nsite].u.hostname);
        if (strspn (site[nsite].u.hostname, " .0123456789") == strlen (site[nsite].u.hostname))
        {
            dmsg ("numeric address detected, looking for name\n");
            addr = inet_addr (site[nsite].u.hostname);
            if (options.dns_lookup)
                remote = gethostbyaddr ((char *)&addr, 4, AF_INET);
            else
                remote = NULL;
            if (remote == NULL)
            {
                dmsg ("gethostbyaddr has failed, errno=%d\n", errno);
                PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_TRYING_IP), site[nsite].u.hostname);
                site[nsite].u.ip = addr;
            }
        }
        else
        {
            dmsg ("hostname detected, looking for number\n");
            remote = gethostbyname (site[nsite].u.hostname);
            if (remote == NULL)
            {
                dmsg ("gethostbyname has failed, errno=%d\n", errno);
                PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_CANNOT_RESOLVE), site[nsite].u.hostname);
                return ERR_FATAL;
            }
        }
        dmsg ("name resolution is done\n");
        if (remote != NULL)
        {
            PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_FOUND), remote->h_name);
            site[nsite].u.ip = *((unsigned long *)(remote->h_addr));
        }
    }

    in_addr1.s_addr = site[nsite].u.ip;
    dmsg ("setup_site: remote IP is %s\n", inet_ntoa (in_addr1));

    site[nsite].set_up = TRUE;

    return ERR_OK;
}

/* -------------------------------------------------------------
 -- SendToServer
 writes command to control connection. if connection is broken,
 closes it, reopens and tries again
 -- parameters:
 retry - whether retries are allowed. if FALSE, will return error
  if connection is broken
 resp - buffer to return full answer. if NULL, do not return it
 s    - format string to send. if NULL, do not send anything
 returns:
 error code. if connection cannot be established, will try until
 successful
 */

RC SendToServer (int nsite, int retry, int *ftpcode, char *resp, char *s, ...)
{
    va_list       args;
    char          buffer[16384];

    if (s != NULL)
    {
        va_start (args, s);
        vsprintf (buffer, /*sizeof(buffer),*/ s, args);
        va_end (args);
        s = buffer;
    }

    return SendToServer2 (nsite, retry, ftpcode, resp, s);
}

/* -------------------------------------------------------------
 same as SendToServer but no parameter substitution */

static int feat_watching = FALSE, sitehelp_watching = FALSE;

RC SendToServer2 (int nsite, int retry, int *ftpcode, char *resp, char *s)
{
    char          buffer[16384];
    int           code, rc;

again:
    dmsg ("net.c: at again2: label\n");

    if (!site[nsite].connected)
    {
        if (!retry) return ERR_TRANSIENT;
        dmsg ("not connected2.\n");
        // don't try to establish new connection just to send QUIT :)
        if (s != NULL && strncmp (s, "QUIT", 4) == 0) return ERR_OK;
        rc = reconnect (nsite);
        if (rc == ERR_CANCELLED || rc == ERR_FATAL) return rc;
        if (rc) goto again;
        // ugly hack to fix PORT command
        if (str_headcmp (s, "PORT ") == 0)
        {
            dmsg ("need to rewrite PORT command1!\n");
            return ERR_FIXPORT;
        }
    }

    if (s != NULL)
    {
        strcpy (buffer, s);
        strcat (buffer, "\r\n");

        dmsg ("entering WriteCC() 2\n");
        rc = WriteControlConnection (nsite, buffer);
        if (rc && !retry) return rc;
        if (rc == ERR_TRANSIENT) goto again;
        if (rc) return rc;
    }

    dmsg ("entering ReadCC() 2\n");

    if (s != NULL && str_headcmp (s, "FEAT") == 0) feat_watching = TRUE;
    if (s != NULL && str_headcmp (s, "HELP SITE") == 0) sitehelp_watching = TRUE;
    code = ReadControlConnection (nsite, buffer);
    feat_watching = FALSE;
    sitehelp_watching = FALSE;
    dmsg ("ReadControlConnection() returned %d 2\n", code);

    // check for timeouts
    if ((code == 4 || code == -1) &&
        (str_stristr (buffer, "timeout") != NULL ||
         str_stristr (buffer, "closing") != NULL ||
         str_stristr (buffer, "disconnect") != NULL))
    {
        CloseControlConnection (nsite);
        if (!retry) return ERR_TRANSIENT;
        if (s != NULL) goto again;
    }

    if (code == -2) return ERR_CANCELLED;
    if (ftpcode != NULL) *ftpcode = code;
    if (resp != NULL) strcpy (resp, buffer);
    if (!site[nsite].connected) return ERR_FATAL;
    return ERR_OK;
}

/* -------------------------------------------------------------------------
 -- OpenControlConnection
 opens control connection to site[nsite]. performs all boring login procedure.
 at the end, user is logged in but directories aren't read yet.
 -- parameters:
 none
 -- returns: RC */
static RC OpenControlConnection (int nsite)
{
    struct sockaddr_in   /*cc,*/ sl;
    int                  rc, opt, sl_len, rsp, rsp1;
    unsigned             port;
    unsigned long        ip;
    char                 buf[4096], *p, *p1, *p2, *public_key, *randomizer;

again:
    dmsg ("begin OpenControlConnection()\n");
    /* close connection first if already connected */
    CloseControlConnection (nsite);

    if (fl_opt.is_unix)
        set_window_name ("connecting to %s...", is_firewalled(nsite) ? options.fire_server : site[nsite].u.hostname);
    else
        set_window_name (MSG(M_CONNECTING), is_firewalled(nsite) ? options.fire_server : site[nsite].u.hostname);

    port = is_firewalled(nsite) ? options.fire_port : site[nsite].u.port;
    ip = is_firewalled(nsite) ? firewall.fwip : site[nsite].u.ip;

    site[nsite].cc_sock = Connect2 (nsite, ip, htons (port));
    if (site[nsite].cc_sock < 0)
    {
        dmsg ("connect() failed; errno = %d\n", errno);
        PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_FAILED_TO_CONNECT), port);
        switch (site[nsite].cc_sock)
        {
        case ERR_TRANSIENT:       goto retry;
        case ERR_CANCELLED:       return ERR_CANCELLED;
        case ERR_FATAL:
        default:                  return ERR_FATAL;
        }
    }
    dmsg ("connect() has been successful\n");

    /*
    p = inet_ntoa (sl.sin_addr);
    dmsg ("getsockname() has completed; new address is %s\n", p);
    struct hostent       *this_machine;

        rc = gethostname (hostname, MAXHOSTNAMELEN);
        if (rc < 0) goto need_hostname;
        fprintf (stderr, "hostname: %s; resolving it...\n", hostname);
        machine = gethostbyname (hostname);
        if (machine == NULL) break;
        fprintf (stderr, "IP: %s\n", machine->h_addr);
        p = inet_ntoa (*((struct in_addr *)machine->h_addr));
        for (i=0; i<License.n_ip_masks; i++)
        if (fnmatch1 (License.ip_masks[i], p, 0) == 1) License.status = LIC_VERIFIED;

        rc = gethostname (hostname, MAXHOSTNAMELEN);
        if (rc < 0) goto need_hostname;
        fprintf (stderr, "hostname: %s\n", hostname);
        machine = gethostbyname (hostname);
        if (machine == NULL) break;
        p = inet_ntoa (*((struct in_addr *)(machine->h_addr)));
        fprintf (stderr, "IP: %s\n", p);
        str_lower (hostname);
        for (i=0; i<License.n_host_masks; i++)
        if (fnmatch1 (License.host_masks[i], hostname, 0) == 1) License.status = LIC_VERIFIED;

        rc = gethostname (hostname, MAXHOSTNAMELEN);
        if (rc < 0) goto need_hostname;
        fprintf (stderr, "hostname: %s\n", hostname);
        str_lower (hostname);
        for (i=0; i<License.n_host_masks; i++)
        if (strcmp (License.host_masks[i], hostname) == 0) License.status = LIC_VERIFIED;

    // licensing checks
    dmsg ("1. License.status = %d\n", License.status);
    if (License.status == LIC_DELAYED && (License.type & LCT_TYPE) == LCT_M_IP)
    {
        License.status = LIC_NO;
        for (i=0; i<License.n_ip_masks; i++)
            if (fnmatch1 (License.ip_masks[i], p, 0) == 1) License.status = LIC_VERIFIED;
    }

    if (License.status == LIC_DELAYED && (License.type & LCT_TYPE) == LCT_S_IP)
    {
        License.status = LIC_NO;
        for (i=0; i<License.n_ip_masks; i++)
            if (strcmp (License.ip_masks[i], p) == 0) License.status = LIC_VERIFIED;
    }
    */

    /* query local machine name. shouldn't fail */
    sl_len = sizeof (struct sockaddr);
    getsockname (site[nsite].cc_sock, (struct sockaddr *) &sl, &sl_len);
    site[nsite].l_ip = sl.sin_addr.s_addr;
    p = inet_ntoa (sl.sin_addr);
    dmsg ("getsockname() has completed; new address is %s\n", p);

    /* enable KEEPALIVE */
    opt = 1;
    setsockopt (site[nsite].cc_sock, SOL_SOCKET, SO_KEEPALIVE,
                (char *) &opt, (int) sizeof(opt));

    site[nsite].connected = 1;

    // read server prompt
    dmsg ("going to read server prompt!\n");
    rc = SendToServer (nsite, FALSE, &rsp, buf, NULL);
    if (rc == ERR_CANCELLED) return ERR_CANCELLED;
    if (rc) goto retry;

    if (strstr (buf, "PKFA-capable") != NULL && site[nsite].features.chall_works == -1) site[nsite].features.chall_works = TRUE;

    if (is_firewalled(nsite))
    {
        if (firewall.password[0] == '\0' &&
            (options.firewall_type == 1 || options.firewall_type == 2 || options.firewall_type == 6))
        {
            if (entryfield (MSG(M_ENTER_FIREWALL_USERID), firewall.userid,
                            sizeof (firewall.userid), 0) == PRESSED_ESC ||
                entryfield (MSG(M_ENTER_FIREWALL_PASSWORD), firewall.password,
                            sizeof (firewall.password), LI_INVISIBLE) == PRESSED_ESC)
            {
                CloseControlConnection (nsite);
                return ERR_CANCELLED;
            }
        }
        switch (options.firewall_type)
        {
        case 1:
            rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s", firewall.userid);
            if (rc) break;
            if (rsp != 2 && rsp != 3) {rc = ERR_FATAL; break;}
            if (rsp == 3)
            {
                rc = SendToServer (nsite, FALSE, &rsp, NULL, "PASS %s", firewall.password);
                if (rc) break;
                if (rsp != 2) {rc = ERR_FATAL; break;}
            }
            rc = SendToServer (nsite, FALSE, &rsp, NULL, "SITE %s", site[nsite].u.hostname);
            break;
        case 2:
            rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s", firewall.userid);
            if (rc) break;
            if (rsp != 2 && rsp != 3) rc = ERR_FATAL;
            if (rsp == 3)
            {
                rc = SendToServer (nsite, FALSE, &rsp, NULL, "PASS %s", firewall.password);
                if (rc) break;
                if (rsp != 2) rc = ERR_FATAL;
            }
            break;
        case 3:
            rc = 0;
            break;
        case 4:
            rc = SendToServer (nsite, FALSE, NULL, NULL, "OPEN %s", site[nsite].u.hostname);
            break;
        case 5:
            break;
        case 6:
            rc = 0;
            break;
        default:
            fly_ask_ok (ASK_WARN, MSG(M_INCORRECT_FIREWALL), options.firewall_type);
            exit (1);
        }
        if (rc) return rc;
    }

    // supply USER information

    if (is_firewalled(nsite) && (options.firewall_type == 2 || options.firewall_type == 3))
    {
        if (options.fwbug1)
        {
            if (site[nsite].u.port == options.defaultport)
                rc = SendToServer (nsite, FALSE, &rsp, NULL, "%s@%s", site[nsite].u.userid, site[nsite].u.hostname);
            else
                rc = SendToServer (nsite, FALSE, &rsp, NULL, "%s@%s:%u", site[nsite].u.userid,
                                   site[nsite].u.hostname, site[nsite].u.port);
        }
        else
        {
            if (site[nsite].u.port == options.defaultport)
                rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s@%s", site[nsite].u.userid, site[nsite].u.hostname);
            else
                rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s@%s:%u", site[nsite].u.userid,
                                   site[nsite].u.hostname, site[nsite].u.port);
        }
    }
    else if (is_firewalled(nsite) && options.firewall_type == 6)
    {
        if (site[nsite].u.port == options.defaultport)
            rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s@%s@%s", site[nsite].u.userid, firewall.userid, site[nsite].u.hostname);
        else
            rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s@%s@%s:%u", site[nsite].u.userid, firewall.userid, site[nsite].u.hostname, site[nsite].u.port);
    }
    else
        rc = SendToServer (nsite, FALSE, &rsp, NULL, "USER %s", site[nsite].u.userid);
    if (rc == ERR_CANCELLED) return rc;
    if (rc) goto retry;
    if (rsp == 4) goto retry;

    // assume if server doesn't allow us to even enter name, it's busy
    if (rsp == 5)
    {
        if (!is_anonymous(nsite) && !(site[nsite].u.flags & URL_FLAG_RETRY)) return ERR_FATAL;
        goto retry;
    }

    // supply PASS information
    if (rsp == 3)
    {
        if (is_firewalled(nsite) && options.firewall_type == 6)
        {
            rc = SendToServer (nsite, FALSE, &rsp1, NULL, "PASS %s@%s", site[nsite].u.password, firewall.password);
        }
        else
        {
            if (!is_anonymous(nsite) && site[nsite].features.chall_works == TRUE)
            {
                rc = SendToServer (nsite, FALSE, &rsp1, buf, "CHAL EG1");
                if (rc) return rc;
                // not recognized?
                if (rsp1 == 5) goto no_chal;
                if (str_stristr (buf, "Public key: RSA1 "))
                {
                    // parse response
                    public_key = strstr (buf, ": RSA1 ") + 7;
                    randomizer = strchr (public_key, ' ');
                    if (public_key == NULL || randomizer == NULL) goto no_chal;
                    *randomizer = '\0';
                    randomizer++;
                    // encrypt the password+randomizer
                    p1 = str_join (site[nsite].u.password, randomizer);
                    p2 = rsa_encode (p1, strlen (p1), public_key);
                    rc = SendToServer (nsite, FALSE, &rsp1, NULL, "PKPS %s", p2);
                    free (p1);
                    free (p2);
                }
                else if (str_stristr (buf, "Public key: EG1 "))
                {
                    // parse response
                    public_key = strstr (buf, ": EG1 ") + 6;
                    randomizer = strchr (public_key, ' ');
                    if (public_key == NULL || randomizer == NULL) goto no_chal;
                    *randomizer = '\0';
                    randomizer++;
                    // encrypt the password+randomizer
                    p1 = str_join (site[nsite].u.password, randomizer);
                    p2 = eg_encode (p1, strlen (p1), public_key);
                    rc = SendToServer (nsite, FALSE, &rsp1, NULL, "PKPS %s", p2);
                    free (p1);
                    free (p2);
                }
                else
                    goto no_chal;
            }
            else
                rc = SendToServer (nsite, FALSE, &rsp1, NULL, "PASS %s", site[nsite].u.password);
        }
        if (rc) return rc;
        switch (rsp1)
        {
        case 3:
        case 4:
            goto retry;
        case 5:
            if (is_anonymous(nsite)) goto retry;
            if (site[nsite].u.flags & URL_FLAG_RETRY) goto retry;
            CloseControlConnection (nsite);
            return ERR_FATAL;
        }
        if (!is_anonymous(nsite))
            psw_add (site[nsite].u.hostname, site[nsite].u.userid, site[nsite].u.password);
    }

    PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_LOGIN), site[nsite].u.userid, site[nsite].u.hostname);

    rc = SendToServer (nsite, FALSE, &rsp, buf, "SYST");
    if (rc == ERR_CANCELLED) return rc;
    if (rc) goto retry;
    if (rsp != 2)
    {
        site[nsite].system_type = SYS_UNIX;
    }
    else
    {
        str_lower (buf);

        if (strstr (buf, "unix"))
        {
            if (strstr (buf, "powerweb"))
                site[nsite].system_type = SYS_POWERWEB;
            else
                site[nsite].system_type = SYS_UNIX;
        }
        else if (strstr (buf, "unknown type: l8"))
            site[nsite].system_type = SYS_UNIX;
        else if (strstr (buf, "os/2") && strstr (buf, "neologic"))
            site[nsite].system_type = SYS_OS2NEOLOGIC;
        else if (strstr (buf, "os/2") && strstr (buf, "hethmon"))
            site[nsite].system_type = SYS_OS2NEOLOGIC;
        else if (strstr (buf, "os/2 operating system"))
            site[nsite].system_type = SYS_IBMOS2FTPD;
        else if (strstr (buf, "betriebssystem os/2"))
            site[nsite].system_type = SYS_IBMOS2FTPD;
        else if (strstr (buf, "Операционная система os/2"))
            site[nsite].system_type = SYS_IBMOS2FTPD;
        else if (strstr (buf, "windows_nt"))
            site[nsite].system_type = SYS_WINNTDOS;
        else if (strstr (buf, "windows2000"))
            site[nsite].system_type = SYS_WINNTDOS;
        else if (strstr (buf, "msdos a n (wftpd"))
            site[nsite].system_type = SYS_WFTPD;
        else if (strstr (buf, "war-ftpd"))
            site[nsite].system_type = SYS_WFTPD;
        else if (strstr (buf, "worldgroup"))
            site[nsite].system_type = SYS_WORLDGROUP;
        else if (strstr (buf, "macos peter's server"))
            site[nsite].system_type = SYS_MACOS_PETER;
        else if (strstr (buf, "a series ftp version"))
            site[nsite].system_type = SYS_UNISYS_A;
        else if (strstr (buf, "os/400"))
            site[nsite].system_type = SYS_AS400;
        else if (strstr (buf, "vms multinet"))
            site[nsite].system_type = SYS_VMS_MULTINET;
        else if (strstr (buf, "vms openvms"))
            site[nsite].system_type = SYS_VMS_UCX;
        else if (strstr (buf, "vms vax/vms"))
            site[nsite].system_type = SYS_VMS_UCX;
        else if (strstr (buf, "madgoat system type"))
            site[nsite].system_type = SYS_VMS_MADGOAT;
        else if (strstr (buf, "mpe/ix"))
            site[nsite].system_type = SYS_MPEIX;
        else if (strstr (buf, "vm is the operating system of this server"))
            site[nsite].system_type = SYS_VMSP;
        else if (strstr (buf, "netware"))
            site[nsite].system_type = SYS_NETWARE;
        else if (strstr (buf, "mvs is the operating system of this server. ftp server is the c-server."))
            site[nsite].system_type = SYS_MVS;
        else if (strstr (buf, "mvs is the operating system of this server."))
            site[nsite].system_type = SYS_MVS_NATIVE;
        else if (strstr (buf, "version: ipad"))
            site[nsite].system_type = SYS_UNIX;
        else
        {
            if (!is_batchmode)
                fly_ask_ok (0, MSG(M_UNKNOWN_SERVER_OS), buf);
            site[nsite].system_type = SYS_UNKNOWN;
        }

        dmsg ("system type = %d\n", site[nsite].system_type);

        if (strstr (buf, "os/2") != NULL) status.use_flags[nsite] = FALSE;
    }

    if (site[nsite].system_type == SYS_AS400)
    {
        rc = SendToServer (nsite, FALSE, &rsp, NULL, "SITE NAMEFMT 1");
        if (rc == ERR_CANCELLED) return rc;
        if (rc) goto retry;
    }

    if (site[nsite].system_type == SYS_VMS_MULTINET ||
        site[nsite].system_type == SYS_VMS_MADGOAT)
    {
        rc = SendToServer (nsite, FALSE, &rsp, NULL, "CWD /");
        if (rc == ERR_CANCELLED) return rc;
        if (rc) goto retry;
    }

    if (options.try_ftpext)
    {
        rc = SendToServer (nsite, TRUE, &rsp, NULL, "FEAT");
        if (rsp == 5) site[nsite].features.has_feat = FALSE;
    }

    if (options.query_bfsattrs)
    {
        SendToServer (nsite, TRUE, &rsp, NULL, "HELP SITE");
        dmsg ("site[nsite].features.has_bfs1 = %d\n", site[nsite].features.has_bfs1);
    }

    /* this also serves as a first check for connection aliveness */
    rc = SetTransferMode (nsite);
    if (rc) return rc;
    if (is_remote_unix(nsite) && !is_anonymous(nsite))
    {
        rc = SendToServer (nsite, FALSE, &rsp, NULL, "SITE UMASK 022");
        if (rc == ERR_CANCELLED) return rc;
        if (rc) goto retry;
    }

    if (site[nsite].system_type == SYS_VMSP || site[nsite].system_type == SYS_VMS_UCX || site[nsite].system_type == SYS_MVS_NATIVE)
    {
        site[nsite].features.unixpaths = FALSE;
    }

    dmsg ("OpenControlConnection -- success\n");
    return ERR_OK;

retry:

    CloseControlConnection (nsite);
    if (options.retry_pause == 0) return ERR_CANCELLED;
    PutLineIntoResp (RT_RESP, nsite, " ");
    PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_RETRYING), options.retry_pause);

    if (getkey (options.retry_pause*1000) == _Esc)
    {
        PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_RETRY_ABORTED));
        return ERR_CANCELLED;
    }
    goto again;

no_chal:
    site[nsite].features.chall_works = FALSE;
    CloseControlConnection (nsite);
    goto again;
}

/* -----------------------------------------------------------------
 -- CloseControlConnection
 closes socket and sets corresponding flag to false. does not clear
 file structure or IP information
 -- parameters:
 none
 -- returns: nothing
*/

void CloseControlConnection (int nsite)
{
    dmsg ("closing cc\n");
    if (site[nsite].connected) PutLineIntoResp (RT_COMM, nsite, MSG(M_RESP_CLOSING));
    if (site[nsite].cc_sock > 0) socket_close (site[nsite].cc_sock);
    site[nsite].cc_sock = -1;
    site[nsite].connected = 0;

    //if (fl_opt.is_unix)
    //    set_window_name ("not connected");
    //else
    //    set_window_name (MSG(M_NOT_CONNECTED));
    return;
}

/* -------------------------------------------------------------
 reads control connection stream. returns response code, or -1
 if no connection
 */

struct _features features1;

struct
{
    char *name;
    int  *vp;
}
feat_table[] =
{
    {"mdtm",        &features1.has_mdtm},
    {"size",        &features1.has_size},
    {"rest stream", &features1.has_rest},
    {"tvfs",        &features1.has_tvfs},
    {"mlst",        &features1.has_mlst},
};

static int ReadControlConnection (int nsite, char *resp)
{
    int              rc, respcode, nb = 0, multiline, key, i;
    char             b, *p, inbuf[8192];
    double           target;
    fd_set           rdfs;
    struct timeval   tv;

    if (!site[nsite].set_up) fly_error ("site isn't set up in RCC");

    multiline = FALSE;
    respcode  = 0;
    do
    {
        p = inbuf;
        while (1)
        {
            target = clock1 () + (double)options.cc_timeout;
            do
            {
                tv.tv_sec = 0;
                tv.tv_usec = 300000;
                FD_ZERO (&rdfs);
                FD_SET (site[nsite].cc_sock, &rdfs);
                rc = select (site[nsite].cc_sock+1, &rdfs, NULL, NULL, &tv);
                if (rc > 0) break;
                while ((key = getkey (0)) != 0)
                {
                    if (key == _Esc &&
                        fly_ask (ASK_YN, MSG(M_DISCONNECT), NULL, site[nsite].u.hostname))
                    {
                        CloseControlConnection (nsite);
                        //if (fl_opt.is_unix)
                        //    set_window_name ("not connected");
                        //else
                        //    set_window_name (MSG(M_NOT_CONNECTED));
                        return -2;
                    }
                }
            }
            while (clock1() < target);
            //dmsg ("out of waiting for bytes; rc = %d after select()", rc);

            if (rc > 0)
            {
                nb = recv (site[nsite].cc_sock, &b, 1, 0);
            }

            if (rc <= 0 || nb <= 0)
            {
                //fly_ask_ok (ASK_WARN, MSG(M_CONNECTION_LOST));
                CloseControlConnection (nsite);
                //if (fl_opt.is_unix)
                //    set_window_name ("not connected");
                //else
                //    set_window_name (MSG(M_NOT_CONNECTED));
                return -1;
            }
            //dmsg ("[%c] at %d\n", b, p-inbuf);

            /* if (nb == 0) continue; */
            if (b == '\r')
            {
                continue;
            }
            if (b == '\n')
            {
                if (p==inbuf) continue;
                else          break;
            }
            *p++ = b;
            if (p >= inbuf+sizeof(inbuf)-1) p--;
        }
        *p = 0;
        //dmsg ("received: [%s]\n", inbuf);
        PutLineIntoResp (RT_RESP, nsite, "%s", inbuf);
        if (feat_watching)
        {
            features1 = site[nsite].features;
            str_lower (inbuf);
            //dmsg ("feat_watching: %s\n", inbuf);
            for (i=0; i<sizeof(feat_table)/sizeof(feat_table[0]); i++)
            {
                if (str_headcmp (inbuf+1, feat_table[i].name) == 0)
                    *(feat_table[i].vp) = TRUE;
            }
            site[nsite].features = features1;
        }
        if (sitehelp_watching)
        {
            if (str_stristr (inbuf, "LSAT") != NULL) site[nsite].features.has_bfs1 = TRUE;
        }
        dmsg ("CC[%d]: received [%s]\n", nsite, inbuf);
        if (multiline)
        {
            if (respcode == (inbuf[0]-'0')*100 + (inbuf[1]-'0')*10 + (inbuf[2]-'0'))
//                inbuf[0]*100 + inbuf[1]*10 + inbuf[2])
                multiline = FALSE;
        }
        else
        {
            respcode = (inbuf[0]-'0')*100 + (inbuf[1]-'0')*10 + (inbuf[2]-'0');
        }
        if (inbuf[3] == '-') multiline = TRUE;
    }
    while (multiline);
    if (resp != NULL) strcpy (resp, inbuf);

    //if (inbuf[0] == '4' && inbuf[1] == '2' && inbuf[2] == '1')
    //    return 7;

    return inbuf[0]-'0';
}

/* -------------------------------------------------------------

 WriteControlConnection: sends string to server
 returns either ERR_OK or ERR_TRANSIENT
 */

static RC WriteControlConnection (int nsite, char *command)
{
    int              nb, rc;
    char             *p;
    double           target;
    fd_set           wrfs;
    struct timeval   tv;

    if (!site[nsite].set_up) fly_error ("internal error. site isn't set up in WCC");

    if (str_stristr (command, "PASS") == NULL)
        dmsg ("CC[%d]: sending |%s", nsite, command);
    else
        dmsg ("CC[%d]: sending password\n", nsite);

    if (fl_opt.platform != PLATFORM_BEOS_TERM)
    {
        target = clock1 () + (double)options.cc_timeout;
        do
        {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO (&wrfs);
            FD_SET (site[nsite].cc_sock, &wrfs);
            rc = select (site[nsite].cc_sock+1, NULL, &wrfs, NULL, &tv);
            if (getkey (0) == _Esc &&
                fly_ask (ASK_YN, MSG(M_DISCONNECT), NULL, site[nsite].u.hostname))
            {
                CloseControlConnection (nsite);
                if (fl_opt.is_unix)
                    set_window_name ("not connected");
                else
                    set_window_name (MSG(M_NOT_CONNECTED));
                return ERR_CANCELLED;
            }
        }
        while (rc == 0 && clock1() < target);
    }
    nb = send (site[nsite].cc_sock, command, strlen (command), 0);

    if (nb == -1) // ok, we have no CC anymore...
    {
        dmsg ("CC[%d] is dead!\n", nsite);
        CloseControlConnection (nsite);
        return ERR_TRANSIENT;
    }

    p = command;
    while (*p != '\r' && *p) p++;
    *p = 0;
    PutLineIntoResp (RT_CMD, nsite, "%s", command);

    return ERR_OK;
}

/* ------------------------------------------------------------- */

void PutLineIntoResp (char type, int nsite, char *format, ...)
{
   va_list       args;
   char          buffer[9000];
   struct tm     t1;

   buffer[0] = type;
   va_start (args, format);
   vsprintf (buffer+1, /*sizeof(buffer),*/ format, args);
   va_end (args);

   if (!strnicmp1 (buffer+1, "PASS ", 5)) strcpy (buffer+6, "*");

   if (site[nsite].CC.na == 0 || site[nsite].CC.n == site[nsite].CC.na)
   {
       site[nsite].CC.na += 1024;
       site[nsite].CC.ptr = realloc (site[nsite].CC.ptr, site[nsite].CC.na * sizeof (site[nsite].CC.ptr[0]));
       site[nsite].CC.tms = realloc (site[nsite].CC.tms, site[nsite].CC.na * sizeof (site[nsite].CC.tms[0]));
   }

   site[nsite].CC.ptr[site[nsite].CC.n] = strdup (buffer);
   site[nsite].CC.tms[site[nsite].CC.n] = time (NULL);
   site[nsite].CC.n++;
   //if (script_log != NULL) fprintf (script_log, "%s\n", buffer+1);

   if (options.log_cc)
   {
       t1 = *localtime (&(site[nsite].CC.tms[site[nsite].CC.n-1]));
       fprintf (cc_log, "[%d] %02d:%02d:%02d %02d/%02d/%04d %s\n",
                nsite,
                t1.tm_mday, t1.tm_mon+1, t1.tm_year+1900,
                t1.tm_hour, t1.tm_min, t1.tm_sec,
                buffer+1);
   }

   //do_show_cc (0);
}
/* ------------------------------------------------------------- */

static int reconnect (int nsite)
{
    int  rc, rsp;

    dmsg ("entering reconnect()\n");

    rc = OpenControlConnection (nsite);
    if (rc) return rc;

    if (site[nsite].u.pathname[0] != '\0')
    {
        rc = SendToServer (nsite, FALSE, &rsp, NULL, "CWD %s", site[nsite].u.pathname);
        if (rc) return rc;
    }

    return ERR_OK;
}

/* ------------------------------------------------------------- */

RC SetTransferMode (int nsite)
{
    if (status.binary)
        return SendToServer (nsite, TRUE, NULL, NULL, "TYPE I");
    else
        return SendToServer (nsite, TRUE, NULL, NULL, "TYPE A");
}

