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
#include "keys.h"
#include "network.h"
#include "scripting.h"
#include "passwords.h"

struct _site             site;
struct _status           status;
struct _cchistory        cchistory = {NULL, NULL, 0, 0, 0, 0};
struct _firewall         firewall;

static RC   OpenControlConnection (void);
static int  ReadControlConnection (char *resp);
static RC   WriteControlConnection (char *command);
void        CloseControlConnection (void);
static RC   Setup_site (url_t *u);
static int  reconnect (void);

static void PutLineIntoResp0 (char *message);

char *rsa_encode1 (char *s, int length, char *key);

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

int Login (url_t *u)
{
    int   rc;
    char  *p;

    // check for empty hostname
    if (u->hostname[0] == '\0') return ERR_CANCELLED;
    
    put_message (M("Connecting to %s..."), u->hostname);
    
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
    else if (u->password[0] == '\0' && !options.delayed_passwords && !(u->flags & URL_FLAG_SKEY))
    {
        p = psw_match (u->hostname, u->userid);
        if (p == NULL)
        {
            if (entryfield (M(" Enter your password : "),
                            u->password, sizeof(u->password),
                            LI_INVISIBLE) == PRESSED_ESC)
                return ERR_CANCELLED;
        }
        else
        {
            str_scopy (u->password, p);
        }
    }
    
    // check: previous attempt failed due to wrong password?
    if (site.set_up &&
        strcmp (u->hostname, site.u.hostname) == 0 &&
        u->port == site.u.port &&
        strcmp (u->userid, site.u.userid) == 0 &&
        strcmp (u->password, site.u.password) != 0)
        strcpy (site.u.password, u->password);
    
    // check: already connected to the same site?
    if (!site.set_up ||
        strcmp (u->hostname, site.u.hostname) != 0 ||
        u->port != site.u.port ||
        strcmp (u->userid, site.u.userid) != 0)
    {
        if (ask_logoff()) return ERR_CANCELLED;

        if (!options.slowlink) set_view_mode (VIEW_CONTROL);
        if (site.connected) CloseControlConnection ();

        rc = Setup_site (u);
        if (rc) return rc;

        if (!status.use_proxy || options.firewall_type != 5)
        {
            rc = OpenControlConnection ();
            if (rc)
            {
                set_window_name (M("Not connected"));
                site.set_up = FALSE;
                return rc;
            }
        }
        else
        {
            //site.system_type = SYS_SQUID;
            site.system_type = SYS_APACHE;
        }
    }

    if (site.system_type == SYS_VMS_MULTINET && u->pathname[0] == '\0')
        strcpy (u->pathname, "/");
    
    if (is_http_proxy && u->pathname[0] == '\0')
        strcpy (u->pathname, "/");

    rc = change_dir (u->pathname) ? change_dir ("") : 0;
    if (rc)
    {
        CloseControlConnection ();
        return rc;
    }

    //set_window_name ("%s - %s", site.u.hostname, site.u.pathname);
    adjust_menu_status ();
    set_view_mode (VIEW_REMOTE);
    
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

int Logoff (void)
{
    if (!site.set_up) return ERR_OK;

    history_add (&site.u);
    CloseControlConnection ();
    
    return ERR_OK;
}
    
/* --------------------------------------------------------------------
 
 -- Setup_site:
 initializes 'site' structure. sets defaults, resolves name
 -- returns: RC */

static RC Setup_site (url_t *u)
{
    struct hostent     *remote;
    struct in_addr     in_addr1;
    unsigned long      addr;

    dmsg ("Setup_site entered; hostname = [%s]\n", u->hostname);

    site.u = *u;

    // fill in defaults
    site.connected     = 0;
    site.cc_sock       = -1; /* for now */
    //site.batch_mode    = FALSE;
    site.system_type   = SYS_UNKNOWN;
    //site.use_PASV      = options.PASV_always;
    site.sortmode      = abs (options.defaultsort);
    site.sortdirection = (options.defaultsort >= 0) ? 1 : -1;
    site.speed         = 0.; /* isn't known yet */
    site.fallback      = FALSE;

    if (site.u.flags & URL_FLAG_PASSIVE)
    {
        status.passive_mode = TRUE;
        adjust_menu_status ();
    }
    site.date_fmt = options.as400_date_fmt;

    site.features.rest_works    = TRUE;
    site.features.appe_works    = TRUE;
    site.features.chmod_works   = TRUE;
    site.features.utime_works   = TRUE;
    site.features.chall_works   = -1;
    site.features.has_feat      = FALSE;
    site.features.has_mdtm      = TRUE;
    site.features.has_size      = FALSE;
    site.features.has_rest      = FALSE;
    site.features.has_tvfs      = FALSE;
    site.features.has_mlst      = FALSE;
    site.features.unixpaths     = TRUE;
    site.features.has_bfs1      = -1;

    // simulate empty directory to prevent uninitialized fields
    site.ndir          = 1;
    site.cdir          = 0;
    site.dir[site.cdir].files  = malloc (sizeof(file) * (1));
    site.dir[site.cdir].name   = "";
    site.dir[site.cdir].nfiles = 0;
    site.dir[site.cdir].first_item = 0;
    site.dir[site.cdir].current_file = 0;
    if (site.chunks != NULL) chunk_free (site.chunks);
    site.chunks = chunk_new (0);

    // fix some things
    site.u.pathname[0] = '\0';
    if (site.u.port == 0) site.u.port = options.defaultport;

    // clean up userid and hostname. password might contain spaces!
    str_strip2 (site.u.hostname, "\n\r\t ");
    str_strip2 (site.u.userid, "\n\r\t ");

    // we're all set. now trying to verify hostnames */

    if (firewall.userid[0] == '\0')
        strcpy (firewall.userid, options.fire_login);
    
    if (firewall.password[0] == '\0')
        strcpy (firewall.password, options.fire_passwd);
    
    if (is_http_proxy)
    {
        site.set_up = 1;
        return ERR_OK;
    }
    
    PutLineIntoResp2 ("");
    if (!is_firewalled && site.u.ip == 0L)
    {
        PutLineIntoResp2 (M("Looking up '%s'"), site.u.hostname);
        if (strspn (site.u.hostname, " .0123456789") == strlen (site.u.hostname))
        {
            dmsg ("numeric address detected, looking for name\n");
            addr = inet_addr (site.u.hostname);
            if (options.dns_lookup)
                remote = gethostbyaddr ((char *)&addr, 4, AF_INET);
            else
                remote = NULL;
            if (remote == NULL)
            {
                dmsg ("gethostbyaddr has failed, errno=%d\n", errno);
                PutLineIntoResp2 (M("Cannot find '%s', trying by IP number"),
                                  site.u.hostname);
                site.u.ip = addr;
            }
        }
        else
        {
            dmsg ("hostname detected, looking for number\n");
            remote = gethostbyname (site.u.hostname);
            if (remote == NULL)
            {
                dmsg ("gethostbyname has failed, errno=%d\n", errno);
                PutLineIntoResp2 (M("Cannot find '%s'"), site.u.hostname);
                return ERR_FATAL;
            }
        }
        dmsg ("name resolution is done\n");
        if (remote != NULL)
        {
            PutLineIntoResp2 (M("Found '%s'"), remote->h_name);
            site.u.ip = *((unsigned long *)(remote->h_addr));
        }
    }

    in_addr1.s_addr = site.u.ip;
    dmsg ("setup_site: remote IP is %s\n", inet_ntoa (in_addr1));

    site.set_up = TRUE;
    
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

RC SendToServer (int retry, int *ftpcode, char *resp, char *s, ...)
{
    va_list       args;
    char          buffer[16384];

    if (s != NULL)
    {
        va_start (args, s);
        vsnprintf1 (buffer, sizeof(buffer), s, args);
        va_end (args);
        s = buffer;
    }
    
    return SendToServer2 (retry, ftpcode, resp, s);
}

/* -------------------------------------------------------------
 same as SendToServer but no parameter substitution */

static int feat_watching = FALSE, sitehelp_watching = FALSE;

RC SendToServer2 (int retry, int *ftpcode, char *resp, char *s)
{
    char          buffer[16384];
    int           code, rc, vmode;

    vmode = display.view_mode;

again:
    dmsg ("net.c: at again2: label\n");

    if (!site.connected)
    {
        if (!retry) return ERR_TRANSIENT;
        dmsg ("not connected2.\n");
        // don't try to establish new connection just to send QUIT :)
        if (s != NULL && strncmp (s, "QUIT", 4) == 0) return ERR_OK;
        //if (!options.slowlink) set_view_mode (VIEW_CONTROL);
        rc = reconnect ();
        if (rc == ERR_CANCELLED || rc == ERR_FATAL) return rc;
        if (rc) goto again;
        // ugly hack to fix PORT command
        if (str_headcmp (s, "PORT ") == 0)
        {
            dmsg ("need to rewrite PORT command1!\n");
            return ERR_FIXPORT;
        }
    }
        
    if (!options.slowlink && options.autocontrol) set_view_mode (VIEW_CONTROL);
    if (s != NULL)
    {
        strcpy (buffer, s);
        strcat (buffer, "\r\n");

        dmsg ("entering WriteCC() 2\n");
        rc = WriteControlConnection (buffer);
        if (rc && !retry) return rc;
        if (rc == ERR_TRANSIENT) goto again;
        if (rc) return rc;
    }
    
    dmsg ("entering ReadCC() 2\n");

    if (s != NULL && str_headcmp (s, "FEAT") == 0) feat_watching = TRUE;
    if (s != NULL && str_headcmp (s, "HELP SITE") == 0) sitehelp_watching = TRUE;
    code = ReadControlConnection (buffer);
    feat_watching = FALSE;
    sitehelp_watching = FALSE;
    dmsg ("ReadControlConnection() returned %d 2\n", code);

    // check for timeouts
    if ((code == 4 || code == -1) &&
        (str_stristr (buffer, "timeout") != NULL ||
         str_stristr (buffer, "closing") != NULL ||
         str_stristr (buffer, "disconnect") != NULL))
    {
        CloseControlConnection ();
        if (!retry) return ERR_TRANSIENT;
        if (s != NULL) goto again;
    }

    display.view_mode = vmode;

    if (code == -2) return ERR_CANCELLED;
    if (ftpcode != NULL) *ftpcode = code;
    if (resp != NULL) strcpy (resp, buffer);
    if (!site.connected) return ERR_FATAL;
    return ERR_OK;
}

/* -------------------------------------------------------------------------
 -- OpenControlConnection
 opens control connection to site. performs all boring login procedure.
 at the end, user is logged in but directories aren't read yet.
 -- parameters:
 none
 -- returns: RC */
static RC OpenControlConnection (void)
{
    struct sockaddr_in   /*cc,*/ sl;
    int                  rc, opt, sl_len, rsp, rsp1;
    unsigned             port;
    unsigned long        ip;
    char                 buf[4096], *p;
    char                 resp33x[4096];

    if (!options.slowlink)
        set_view_mode (VIEW_CONTROL);

again:
    dmsg ("begin OpenControlConnection()\n");
    /* close connection first if already connected */
    CloseControlConnection ();

    if (fl_opt.is_unix)
        set_window_name ("connecting to %s...", is_firewalled ? options.fire_server : site.u.hostname);
    else
        set_window_name (M("Connecting to %s..."), is_firewalled ? options.fire_server : site.u.hostname);

    port = is_firewalled ? options.fire_port : site.u.port;
    ip = is_firewalled ? firewall.fwip : site.u.ip;

    site.cc_sock = Connect2 (ip, htons (port));
    if (site.cc_sock < 0)
    {
        dmsg ("connect() failed; errno = %d\n", errno);
        PutLineIntoResp2 (M("Failed to connect to port %u"), port);
        switch (site.cc_sock)
        {
        case ERR_TRANSIENT:       goto retry;
        case ERR_CANCELLED:       return ERR_CANCELLED;
        case ERR_FATAL:
        default:                  return ERR_FATAL;
        }
    }
    //PutLineIntoResp2 (MSG(M_RESP_CONNECTED), port);
    dmsg ("connect() has been successful\n");

    /* query local machine name. shouldn't fail */
    sl_len = sizeof (struct sockaddr);
    if (getsockname (site.cc_sock, (struct sockaddr *) &sl, &sl_len) < 0)
        goto again;
    site.l_ip = sl.sin_addr.s_addr;
    p = inet_ntoa (sl.sin_addr);
    dmsg ("getsockname() has completed; new address is %s\n", p);

    /* enable KEEPALIVE */
    opt = 1;
    setsockopt (site.cc_sock, SOL_SOCKET, SO_KEEPALIVE,
                (char *) &opt, (int) sizeof(opt));
    
    site.connected = 1;
    
    // read server prompt
    dmsg ("going to read server prompt!\n");
    rc = SendToServer (FALSE, &rsp, buf, NULL);
    if (rc == ERR_CANCELLED) return ERR_CANCELLED;
    if (rc) goto retry;

    if (is_firewalled)
    {
        if (firewall.password[0] == '\0' &&
            (options.firewall_type == 1 || options.firewall_type == 2 || options.firewall_type == 6))
        {
            if (entryfield (M(" Enter your FIREWALL user id: "),
                            firewall.userid,
                            sizeof (firewall.userid), 0) == PRESSED_ESC ||
                entryfield (M(" Enter your FIREWALL password: "),
                            firewall.password,
                            sizeof (firewall.password), LI_INVISIBLE) == PRESSED_ESC)
            {
                CloseControlConnection ();
                return ERR_CANCELLED;
            }
        }
        switch (options.firewall_type)
        {
        case 1:
            rc = SendToServer (FALSE, &rsp, NULL, "USER %s", firewall.userid);
            if (rc) break;
            if (rsp != 2 && rsp != 3) {rc = ERR_FATAL; break;}
            if (rsp == 3)
            {
                rc = SendToServer (FALSE, &rsp, NULL, "PASS %s", firewall.password);
                if (rc) break;
                if (rsp != 2) {rc = ERR_FATAL; break;}
            }
            rc = SendToServer (FALSE, &rsp, NULL, "SITE %s", site.u.hostname);
            break;
        case 2:
            rc = SendToServer (FALSE, &rsp, NULL, "USER %s", firewall.userid);
            if (rc) break;
            if (rsp != 2 && rsp != 3) rc = ERR_FATAL;
            if (rsp == 3)
            {
                rc = SendToServer (FALSE, &rsp, NULL, "PASS %s", firewall.password);
                if (rc) break;
                if (rsp != 2) rc = ERR_FATAL;
            }
            break;
        case 3:
            rc = 0;
            break;
        case 4:
            rc = SendToServer (FALSE, NULL, NULL, "OPEN %s", site.u.hostname);
            break;
        case 5:
            break;
        case 6:
            rc = 0;
            break;
        default:
            fly_ask_ok (ASK_WARN,
                        M(" Incorrent firewall type specified : %d "),
                        options.firewall_type);
            exit (1);
        }
        if (rc) return rc;
    }
    
    // supply USER information

    if (is_firewalled && (options.firewall_type == 2 || options.firewall_type == 3))
    {
        if (options.fwbug1)
        {
            if (site.u.port == options.defaultport)
                rc = SendToServer (FALSE, &rsp, resp33x, "%s@%s", site.u.userid, site.u.hostname);
            else
                rc = SendToServer (FALSE, &rsp, resp33x, "%s@%s:%u", site.u.userid,
                                   site.u.hostname, site.u.port);
        }
        else
        {
            if (site.u.port == options.defaultport)
                rc = SendToServer (FALSE, &rsp, resp33x, "USER %s@%s", site.u.userid, site.u.hostname);
            else
                rc = SendToServer (FALSE, &rsp, resp33x, "USER %s@%s:%u", site.u.userid,
                                   site.u.hostname, site.u.port);
        }
    }
    else if (is_firewalled && options.firewall_type == 6)
    {
        if (site.u.port == options.defaultport)
            rc = SendToServer (FALSE, &rsp, resp33x, "USER %s@%s@%s", site.u.userid, firewall.userid, site.u.hostname);
        else
            rc = SendToServer (FALSE, &rsp, resp33x, "USER %s@%s@%s:%u", site.u.userid, firewall.userid, site.u.hostname, site.u.port);
    }
    else
        rc = SendToServer (FALSE, &rsp, resp33x, "USER %s", site.u.userid);
    if (rc == ERR_CANCELLED) return rc;
    if (rc) goto retry;
    if (rsp == 4) goto retry;

    // assume if server doesn't allow us to even enter name, it's busy
    if (rsp == 5)
    {
        if (site.u.flags & URL_FLAG_RETRY) goto retry;
        CloseControlConnection ();
        return ERR_FATAL;
    }
    
    // supply PASS information
    if (rsp == 3)
    {
        if (site.u.password[0] == '\0' && (options.delayed_passwords || site.u.flags & URL_FLAG_SKEY))
        {
            //if (entryfield (MSG(M_ENTER_PASSWORD), site.u.password, sizeof(site.u.password),
            if (entryfield (resp33x, site.u.password, sizeof(site.u.password),
                            LI_INVISIBLE) == PRESSED_ESC)
            {
                CloseControlConnection ();
                return ERR_CANCELLED;
            }
        }
        if (is_firewalled && options.firewall_type == 6)
        {
            rc = SendToServer (FALSE, &rsp1, NULL, "PASS %s@%s", site.u.password, firewall.password);
        }
        else
        {
            rc = SendToServer (FALSE, &rsp1, NULL, "PASS %s", site.u.password);
        }
        if (rc) return rc;
        switch (rsp1)
        {
        case 3:
        case 4:
            goto retry;
        case 5:
            if (is_anonymous) goto retry;
            if (site.u.flags & URL_FLAG_RETRY) goto retry;
            CloseControlConnection ();
            return ERR_FATAL;
        }
        if (!is_anonymous)
            psw_add (site.u.hostname, site.u.userid, site.u.password);
    }

    PutLineIntoResp2 (M("Successfully logged in as '%s@%s'"),
                      site.u.userid, site.u.hostname);

    rc = SendToServer (FALSE, &rsp, buf, "SYST");
    if (rc == ERR_CANCELLED) return rc;
    if (rc) goto retry;
    if (rsp != 2)
    {
        site.system_type = SYS_UNIX;
    }
    else
    {
        str_lower (buf);
        
        if (strstr (buf, "unix"))
        {
            if (strstr (buf, "powerweb"))
                site.system_type = SYS_POWERWEB;
            else
                site.system_type = SYS_UNIX;
        }
        else if (strstr (buf, "unknown type: l8"))
            site.system_type = SYS_UNIX;
        else if (strstr (buf, "os/2") && strstr (buf, "neologic"))
            site.system_type = SYS_OS2NEOLOGIC;
        else if (strstr (buf, "os/2") && strstr (buf, "hethmon"))
            site.system_type = SYS_OS2NEOLOGIC;
        else if (strstr (buf, "os/2 operating system"))
            site.system_type = SYS_IBMOS2FTPD;
        else if (strstr (buf, "betriebssystem os/2"))
            site.system_type = SYS_IBMOS2FTPD;
        else if (strstr (buf, "Операционная система os/2"))
            site.system_type = SYS_IBMOS2FTPD;
        else if (strstr (buf, "windows_nt")) 
            site.system_type = SYS_WINNTDOS;
        else if (strstr (buf, "windows2000"))
            site.system_type = SYS_WINNTDOS;
        else if (strstr (buf, "msdos a n (wftpd"))
            site.system_type = SYS_WFTPD;
        else if (strstr (buf, "war-ftpd"))
            site.system_type = SYS_WFTPD;
        else if (strstr (buf, "worldgroup"))
            site.system_type = SYS_WORLDGROUP;
        else if (strstr (buf, "macos peter's server"))
            site.system_type = SYS_MACOS_PETER;
        else if (strstr (buf, "a series ftp version"))
            site.system_type = SYS_UNISYS_A;
        else if (strstr (buf, "os/400"))
            site.system_type = SYS_AS400;
        else if (strstr (buf, "vms multinet"))
            site.system_type = SYS_VMS_MULTINET;
        else if (strstr (buf, "vms openvms"))
            site.system_type = SYS_VMS_UCX;
        else if (strstr (buf, "vms vax/vms"))
            site.system_type = SYS_VMS_UCX;
        else if (strstr (buf, "madgoat system type"))
            site.system_type = SYS_VMS_MADGOAT;
        else if (strstr (buf, "mpe/ix"))
            site.system_type = SYS_MPEIX;
        else if (strstr (buf, "vm is the operating system of this server"))
            site.system_type = SYS_VMSP;
        else if (strstr (buf, "netware"))
            site.system_type = SYS_NETWARE;
        else if (strstr (buf, "mvs is the operating system of this server. ftp server is the c-server."))
            site.system_type = SYS_MVS;
        else if (strstr (buf, "mvs is the operating system of this server."))
            site.system_type = SYS_MVS_NATIVE;
        else if (strstr (buf, "version: ipad"))
            site.system_type = SYS_UNIX;
        else if (strstr (buf, "windows nt 5.0"))
            site.system_type = SYS_UNIX;
        else if (strstr (buf, "windows 95 4.0"))
            site.system_type = SYS_UNIX;
        else
        {
            //if (!is_batchmode)
            //    fly_ask_ok (0, MSG(M_UNKNOWN_SERVER_OS), buf);
            PutLineIntoResp2 (M(
                                " Warning: unknown operating system on ftp server: \n"
                                " '%s'; \n"
                                " NFTP might fail to parse listing "), buf);
            site.system_type = SYS_UNKNOWN;
        }

        dmsg ("system type = %d\n", site.system_type);
        /* ================================ */
        //site.system_type = SYS_MVS_NATIVE;
        /* ================================ */

        if (strstr (buf, "os/2") != NULL) status.use_flags = FALSE;
    }

    if (site.system_type == SYS_AS400)
    {
        rc = SendToServer (FALSE, &rsp, NULL, "SITE NAMEFMT 1");
        if (rc == ERR_CANCELLED) return rc;
        if (rc) goto retry;
    }
    
    if (site.system_type == SYS_VMS_MULTINET ||
        site.system_type == SYS_VMS_MADGOAT)
    {
        rc = SendToServer (FALSE, &rsp, NULL, "CWD /");
        if (rc == ERR_CANCELLED) return rc;
        if (rc) goto retry;
    }

    if (options.try_ftpext)
    {
        rc = SendToServer (TRUE, &rsp, NULL, "FEAT");
        if (rsp == 5) site.features.has_feat = FALSE;
    }

    if (options.query_bfsattrs)
    {
        SendToServer (TRUE, &rsp, NULL, "HELP SITE");
        dmsg ("site.features.has_bfs1 = %d\n", site.features.has_bfs1);
    }

    /* this also serves as a first check for connection aliveness */
    rc = SetTransferMode ();
    if (rc) return rc;
    if (is_remote_unix && !is_anonymous)
    {
        rc = SendToServer (FALSE, &rsp, NULL, "SITE UMASK 022");
        if (rc == ERR_CANCELLED) return rc;
        if (rc) goto retry;
    }

    if (site.system_type == SYS_VMSP || site.system_type == SYS_VMS_UCX || site.system_type == SYS_MVS_NATIVE)
    {
        site.features.unixpaths = FALSE;
    }
    
    dmsg ("OpenControlConnection -- success\n");
    return ERR_OK;

retry:

    CloseControlConnection ();
    if (options.retry_pause == 0) return ERR_CANCELLED;
    PutLineIntoResp1 (" ");
    PutLineIntoResp2 (M("Pausing for %d seconds... "
                        "ESC to interrupt, other key to retry now"), options.retry_pause);
    
    if (getkey (options.retry_pause*1000) == _Esc)
    {
        PutLineIntoResp2 (M("Aborted."));
        return ERR_CANCELLED;
    }
    goto again;

    CloseControlConnection ();
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

void CloseControlConnection (void)
{
    dmsg ("closing cc\n");
    if (site.connected) PutLineIntoResp2 (M("Closing control connection"));
    if (site.cc_sock > 0) socket_close (site.cc_sock);
    site.cc_sock = -1;
    site.connected = 0;
    //if (fl_opt.is_unix)
    //    set_window_name ("not connected");
    //else
        set_window_name (M("Not connected"));
    return;
}

/* -------------------------------------------------------------
 reads control connection stream. returns response code, or -1
 if no connection
 */

struct
{
    char *name;
    int  *value;
}
feat_table[] =
{
    {"mdtm", &site.features.has_mdtm},
    {"size", &site.features.has_size},
    {"rest stream", &site.features.has_rest},
    {"tvfs", &site.features.has_tvfs},
    {"mlst", &site.features.has_mlst},
};

static int ReadControlConnection (char *resp)
{
    int              rc, respcode, nb = 0, multiline, key, i;
    char             b, *p, inbuf[8192];
    double           target;
    fd_set           rdfs;
    struct timeval   tv;

    if (!site.set_up) fly_error ("site isn't set up in RCC");

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
                FD_SET (site.cc_sock, &rdfs);
                rc = select (site.cc_sock+1, &rdfs, NULL, NULL, &tv);
                if (rc > 0) break;
                while ((key = getkey (0)) != 0)
                {
                    if (key == _Esc &&
                        fly_ask (ASK_YN,
                                 M("  Break connection with server %s?  "),
                                 NULL, site.u.hostname))
                    {
                        CloseControlConnection ();
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
                nb = recv (site.cc_sock, &b, 1, 0);
            }
            
            if (rc <= 0 || nb <= 0)
            {
                //fly_ask_ok (ASK_WARN, MSG(M_CONNECTION_LOST));
                CloseControlConnection ();
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
        PutLineIntoResp1 ("%s", inbuf);
        if (feat_watching)
        {
            str_lower (inbuf);
            //dmsg ("feat_watching: %s\n", inbuf);
            for (i=0; i<sizeof(feat_table)/sizeof(feat_table[0]); i++)
            {
                if (str_headcmp (inbuf+1, feat_table[i].name) == 0)
                    *(feat_table[i].value) = TRUE;
            }
        }
        if (sitehelp_watching)
        {
            if (str_stristr (inbuf, "LSAT") != NULL) site.features.has_bfs1 = TRUE;
        }
        dmsg ("CC: received [%s]\n", inbuf);
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
    if (display.view_mode == VIEW_CONTROL) update (1);
    
    //if (inbuf[0] == '4' && inbuf[1] == '2' && inbuf[2] == '1')
    //    return 7;

    return inbuf[0]-'0';
}

/* -------------------------------------------------------------
 
 WriteControlConnection: sends string to server
 returns either ERR_OK or ERR_TRANSIENT
 */

static RC WriteControlConnection (char *command)
{
    int              nb, rc;
    char             *p;
    double           target;
    fd_set           wrfs;
    struct timeval   tv;

    if (!site.set_up) fly_error ("internal error. site isn't set up in WCC");

    if (str_stristr (command, "PASS") == NULL)
        dmsg ("CC: sending |%s", command);
    else
        dmsg ("CC: sending password\n");

    if (fl_opt.platform != PLATFORM_BEOS_TERM)
    {
        target = clock1 () + (double)options.cc_timeout;
        do
        {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO (&wrfs);
            FD_SET (site.cc_sock, &wrfs);
            rc = select (site.cc_sock+1, NULL, &wrfs, NULL, &tv);
            if (getkey (0) == _Esc &&
                fly_ask (ASK_YN, M("  Break connection with server %s?  "), NULL, site.u.hostname))
            {
                CloseControlConnection ();
                if (fl_opt.is_unix)
                    set_window_name ("not connected");
                else
                    set_window_name (M("Not connected"));
                return ERR_CANCELLED;
            }
        }
        while (rc == 0 && clock1() < target);
    }
    nb = send (site.cc_sock, command, strlen (command), 0);

    if (nb == -1) // ok, we have no CC anymore...
    {
        dmsg ("CC is closed!\n");
        CloseControlConnection ();
        return ERR_TRANSIENT;
    }
    
    p = command;
    while (*p != '\r' && *p) p++;
    *p = 0;
    PutLineIntoResp3 ("%s", command);
    if (display.view_mode == VIEW_CONTROL) update (1);
    
    return ERR_OK;
}

/* ------------------------------------------------------------- */

void DisplayConnectionHistory (void)
{
   char       buf[1024], *p;
   int        n0, line;
   char       attr;
   struct tm  *t1;

   if (cchistory.fline == -1)
      n0 = max1 (cchistory.n - p_nl(), 0);
   else
      n0 = cchistory.fline;

   for (line=p_sl(); line < p_nl()+p_sl(); line++)
   {
      if (n0+line-p_sl() < cchistory.n)
      {
          p = cchistory.ptr[n0+line-p_sl()];
          if (!strncmp (p, "(-)", 3))
          {
              attr = options.attr_cntr_cmd;
              p += 3;
          }
          else if (!strncmp (p, "[-]", 3))
          {
              attr = options.attr_cntr_comment;
              p += 3;
          }
          else
              attr = options.attr_cntr_resp;
          
          memset (buf, ' ', sizeof (buf));
          if (strlen (p) > cchistory.shift)
              strncpy (buf, p+cchistory.shift, video_hsize());
          if (options.show_cc_time)
          {
              video_put_str_attr (buf, video_hsize(), line, 9, attr);
              t1 = localtime (&cchistory.tms[n0+line-p_sl()]);
              snprintf1 (buf, sizeof(buf), "%02d:%02d:%02d ", t1->tm_hour, t1->tm_min, t1->tm_sec);
              video_put_str_attr (buf, 9, line, 0, options.attr_cntr_resp);
          }
          else
          {
              video_put_str_attr (buf, video_hsize(), line, 0, attr);
          }
      }
      else
          video_put_n_cell (' ', options.attr_cntr_resp, video_hsize(), line, 0);
   }
}

/* ------------------------------------------------------------- */

static void PutLineIntoResp0 (char *message)
{
   if (!strnicmp1 (message, "(-)PASS ", 8))    strcpy (message+8, "*");
   //if (!strnicmp1 (message, "(-)PKPASS ", 10)) strcpy (message+10, "*");

   if (cchistory.n == 0 || cchistory.n == cchistory.na)
   {
       cchistory.na += 1024;
       cchistory.ptr = realloc (cchistory.ptr, cchistory.na * sizeof (cchistory.ptr[0]));
       cchistory.tms = realloc (cchistory.tms, cchistory.na * sizeof (cchistory.tms[0]));
   }
   
   cchistory.ptr[cchistory.n] = strdup (message);
   cchistory.tms[cchistory.n] = time (NULL);
   cchistory.fline = -1;
   cchistory.n++;
   if (display.view_mode == VIEW_CONTROL) update (1);
}

/* ------------------------------------------------------------- */

void PutLineIntoResp1 (char *format, ...)
{
   va_list       args;
   char          buffer[9000];

   va_start (args, format);
   vsnprintf1 (buffer, sizeof(buffer), format, args);
   va_end (args);

   PutLineIntoResp0 (buffer);
   if (script_log != NULL) fprintf (script_log, "%s\n", buffer);
}

/* ------------------------------------------------------------- */

void PutLineIntoResp2 (char *format, ...)
{
   va_list       args;
   char          buffer[9000];

   strcpy (buffer, "[-]");
   va_start (args, format);
   vsnprintf1 (buffer+3, sizeof(buffer)-3, format, args);
   va_end (args);

   PutLineIntoResp0 (buffer);
   if (script_log != NULL) fprintf (script_log, "%s\n", buffer+3);
}

/* ------------------------------------------------------------- */

void PutLineIntoResp3 (char *format, ...)
{
   va_list       args;
   char          buffer[9000];

   strcpy (buffer, "(-)");
   va_start (args, format);
   vsnprintf1 (buffer+3, sizeof(buffer)-3, format, args);
   va_end (args);

   PutLineIntoResp0 (buffer);
   if (script_log != NULL) fprintf (script_log, "%s\n", buffer+3);
}

/* ------------------------------------------------------------- */

static int reconnect (void)
{
    int  rc, rsp;

    dmsg ("entering reconnect()\n");
    //set_view_mode (VIEW_CONTROL);

    rc = OpenControlConnection ();
    if (rc) return rc;
    
    if (site.u.pathname[0] != '\0')
    {
        rc = SendToServer (FALSE, &rsp, NULL, "CWD %s", site.u.pathname);
        if (rc) return rc;
    }
    
    return ERR_OK;
}
