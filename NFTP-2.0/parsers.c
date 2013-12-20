#include <fly/fly.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "nftp.h"
#include "net.h"
#include "auxprogs.h"

#define find_space(s,e) {e=s; while(*e && *e!=' ')e++; if (!*e) goto skip;}
#define skip_spaces(s)  {while(*s && *s==' ')s++;if (!*s) goto skip;}

static int LRECL = 1;

static int parse_unix (char *line, file *f);
static int parse_ibm (char *line, file *f);
static int parse_neologic (char *line, file *f);
static int parse_oldnt (char *line, file *f);
static int parse_unisys (char *line, file *f);
static int parse_macos_peter (char *line, file *f);
static int parse_as400 (int nsite, char *line, file *f);
static int parse_vms (char *line, file *f);
static int parse_mpeix (char *line, file *f);
static int parse_squid (char *line, file *f);
static int parse_nsproxy (char *line, file *f);
static int parse_bordermanager (char *line, file *f);
static int parse_mlst (char *line, file *f);
static int parse_vmsp (char *line, file *f);
static int parse_netware (char *line, file *f);
static int parse_mvs1 (char *line, file *f);
static int parse_mvs2 (char *line, file *f);
static int parse_inktomi (char *line, file *f);
static int parse_apache (char *line, file *f);

/* ------------------------------------------------------------- */

static inline unsigned long extract_long (char *s, int n1, int n2)
{
    s[n2] = '\0';
    return atol (s + n1);
}

/* ------------------------------------------------------------- */

static inline unsigned long extract_int64 (char *s, int n1, int n2)
{
    s[n2] = '\0';
    return strtoq1 (s + n1, NULL, 10);
}

/* ------------------------------------------------------------- */

static inline unsigned char *extract_string (char *s, int n1, int n2)
{
    if (n2 != 0) s[n2] = '\0';
    return strdup (s + n1);
}

/* ------------------------------------------------------------- */

void analyze_listing (int nsite, char *buf, int dir_no, char *dirname, int type)
{
    char    *p, *p1, *p2, line[1024], *prevdir;
    int     i, lines, nfil=0, rc, reversed;

    //put_message (MSG(M_PARSING_LISTING));

    prevdir = RN_CURDIR.name;

    dmsg ("analyze_listing (%s)\n", dirname);
    if (site[nsite].system_type == SYS_MVS_NATIVE)
    {
        if (strstr (buf, "Recfm Lrecl BlkSz Dsorg") != NULL)
        {
            type = SYS_MVS_NATIVE1;
        }
        else if (strstr (buf, "Name") != NULL &&
                 strstr (buf, "VV.MM") != NULL &&
                 strstr (buf, "Created") != NULL)
        {
            type = SYS_MVS_NATIVE2;
        }
        else
        {
            type = SYS_UNIX;
        }
        // lookup the directory we're going into in the current and find the LRECL
        for (i=0; i<RN_NF; i++)
        {
            if (str_tailcmp (dirname, RN_FILE(i).name) == 0) break;
        }
        if (i != RN_NF) LRECL = RN_FILE(i).lrecl;
        else              LRECL = 1;
        dmsg ("MVS: type=%d, LRECL=%d\n", type, LRECL);
    }

    // count lines, allocate storage
    dmsg ("got buffer to analyze: [%s]\n", buf);
    p1 = buf; lines = 0;
    while (*p1++)
    {
	if (*p1 == '\r' || *p1 == '\n')
	{
	    lines++;
	    while (*p1 == '\r' || *p1 == '\n') p1++;
	}
    }

    if (dir_no == -1)
    {
        if (site[nsite].ndir == site[nsite].maxndir-1)
        {
            site[nsite].maxndir *= 2;
            site[nsite].dir = realloc (site[nsite].dir, sizeof(directory)*site[nsite].maxndir);
        }
        site[nsite].cdir = site[nsite].ndir;
    }
    else
    {
	free (site[nsite].dir[site[nsite].cdir].files);
    }
    site[nsite].dir[site[nsite].cdir].files = malloc (sizeof(file) * (lines+2));

    /* parse */
    p1   = buf;
    p2   = buf;
    nfil = 0;

    while (nfil < lines+2)
    {
        // extract next line
        while (*p2 && *p2 != '\r' && *p2 != '\n') p2++;
        if (!*p2) break;
        *p2 = 0;
        str_scopy (line, p1);
        if (strlen (line)==0 || *line==26 || strlen(line)==1) goto skip;

        RN_FILE(nfil).rawtext = strdup (line);

        rc = 0;
        dmsg ("parsing: [%s]\n", line);

        RN_FILE(nfil).no = nfil;
        RN_FILE(nfil).flags = 0;
        RN_FILE(nfil).rights = 0644;
        RN_FILE(nfil).mtime = 0;
        RN_FILE(nfil).cached = NULL;
        RN_FILE(nfil).desc = NULL;
        RN_FILE(nfil).owner = NULL;
        RN_FILE(nfil).group = NULL;
        RN_FILE(nfil).lrecl = 0;

        switch (type)
        {
        case SYS_IBMOS2FTPD:
            rc = parse_ibm (line, &(RN_FILE(nfil)));
            break;

        case SYS_OS2NEOLOGIC:
            rc = parse_neologic (line, &(RN_FILE(nfil)));
            break;

        case SYS_WINNTDOS:
            rc = parse_oldnt (line, &(RN_FILE(nfil)));
            break;

        case SYS_UNISYS_A:
            rc = parse_unisys (line, &(RN_FILE(nfil)));
            break;

        case SYS_MACOS_PETER:
            rc = parse_macos_peter (line, &(RN_FILE(nfil)));
            break;

        case SYS_AS400:
            rc = parse_as400 (nsite, line, &(RN_FILE(nfil)));
            break;

        case SYS_VMS_UCX:
            rc = parse_vms (line, &(RN_FILE(nfil)));
            break;

        case SYS_MPEIX:
            rc = parse_mpeix (line, &(RN_FILE(nfil)));
            break;

        case SYS_SQUID:
            rc = parse_squid (line, &(RN_FILE(nfil)));
            break;

        case SYS_NSPROXY:
            rc = parse_nsproxy (line, &(RN_FILE(nfil)));
            break;

        case SYS_BORDERMANAGER:
            rc = parse_bordermanager (line, &(RN_FILE(nfil)));
            break;

        case SYS_MLST:
            rc = parse_mlst (line, &(RN_FILE(nfil)));
            break;

        case SYS_VMSP:
            rc = parse_vmsp (line, &(RN_FILE(nfil)));
            break;

        case SYS_NETWARE:
            rc = parse_netware (line, &(RN_FILE(nfil)));
            break;

        case SYS_MVS_NATIVE1:
            rc = parse_mvs1 (line, &(RN_FILE(nfil)));
            break;

        case SYS_MVS_NATIVE2:
            rc = parse_mvs2 (line, &(RN_FILE(nfil)));
            break;

        case SYS_INKTOMI:
            rc = parse_inktomi (line, &(RN_FILE(nfil)));
            break;

        case SYS_APACHE:
            rc = parse_apache (line, &(RN_FILE(nfil)));
            break;

        default:
        case SYS_UNIX          :
        case SYS_WFTPD         :
        case SYS_POWERWEB      :
        case SYS_WORLDGROUP    :
        case SYS_WINNT         :
        case SYS_UNKNOWN       :
        case SYS_VMS_MULTINET  :
        case SYS_VMS_MADGOAT   :
        case SYS_MVS           :
            rc = parse_unix (line, &(RN_FILE(nfil)));
            if (site[nsite].system_type == SYS_VMS_MULTINET)
            {
                if (options.vms_remove_version && str_tailcmp (RN_FILE(nfil).name, ";1") == 0)
                    *(strstr (RN_FILE(nfil).name, ";1")) = '\0';
            }
        }
        if (rc) goto skip;

	// skip those stupid "."

        if (!strcmp (RN_FILE(nfil).name, ".")) goto skip;
        if (!strcmp (RN_FILE(nfil).name, "..") && strlen (dirname)==1) goto skip;

        // search for extension

        p = strrchr (RN_FILE(nfil).name, '.');
        if (p == NULL || p == RN_FILE(nfil).name)
            RN_FILE(nfil).extension = strlen (RN_FILE(nfil).name);
        else
            RN_FILE(nfil).extension = p + 1 - RN_FILE(nfil).name;

        //if (options.use_UTC && RN_FILE(nfil).mtime > 0)
        //{
        //    RN_FILE(nfil).mtime = gm2local (RN_FILE(nfil).mtime);
        //}

        nfil++;

    skip : // label: skip this line, do not add it to table

        p1 = p2 + 1;
	// skip '\n's and '\r's at the end of line
        while (*p1 && (*p1 == '\r' || *p1 == '\n')) p1++;
        if (!*p1) break;
        p2 = p1;
    }

    // put ".." into the table if needed
    for (i=0; i<nfil; i++)
    {
        if (strcmp (RN_FILE(i).name, "..") == 0) break;
    }
    if (i == nfil || nfil == 0)
    {
        RN_FILE(nfil).rawtext = strdup ("Upper-level directory");
        RN_FILE(nfil).flags = FILE_FLAG_DIR;
        RN_FILE(nfil).size = 0;
        //set_timestamp (&(RN_FILE(nfil).timestamp));
        RN_FILE(nfil).mtime = 0;
        RN_FILE(nfil).name = strdup ("..");
        RN_FILE(nfil).no = 0;
        RN_FILE(nfil).extension = strlen (RN_FILE(0).name);
        RN_FILE(nfil).cached = NULL;
        RN_FILE(nfil).desc = NULL;
        nfil++;
    }

    RN_NF = nfil;
    if (dir_no == -1) site[nsite].ndir++;
    RN_CURRENT = 0;
    RN_FIRST = 0;

    // pointer is to point at .. initially
    for (i=0; i<RN_NF; i++)
    {
        if (strcmp (RN_FILE(i).name, "..") == 0)
        {
            RN_CURRENT = i;
            RN_FIRST = max1 (RN_CURRENT-p_nlf()+1, 0);
        }
    }

    // going up?
    if (prevdir != NULL && strlen (dirname) < strlen(prevdir) &&
        strncmp (dirname, prevdir, strlen(dirname)) == 0)
    {
        p = strrchr (prevdir, '/');
        if (p != NULL)
        {
            for (i=0; i<RN_NF; i++)
                if (strcmp (p+1, RN_FILE(i).name) == 0)
                {
                    RN_CURRENT = i;
                    RN_FIRST = max1 (RN_CURRENT-p_nlf()+1, 0);
                }
        }
    }

    // check for day/month order
    reversed = FALSE;
    /*
    if (type == TYPE_AS400)
    {
        for (i=0; i<RN_NF; i++)
            if (RN_FILE(i).timestamp.tm_mon > 11) reversed = TRUE;
        if (reversed)
        {
            for (i=0; i<RN_NF; i++)
            {
                tmp = RN_FILE(i).timestamp.tm_mon;
                RN_FILE(i).timestamp.tm_mon = RN_FILE(i).timestamp.tm_mday-1;
                RN_FILE(i).timestamp.tm_mday = tmp+1;
            }
            if (site[nsite].date_fmt == DATE_FMT_3) site[nsite].date_fmt = DATE_FMT_2;
            else if (site[nsite].date_fmt == DATE_FMT_2) site[nsite].date_fmt = DATE_FMT_3;
        }
    }
    */
}

/* -------------------------------------------------------------
150 Opening ASCII mode data connection for /bin/ls.
total 583
drwxr-xr-x  11 4461     4460         1024 Oct 11 02:04 .
drwxr-xr-x  11 4461     4460         1024 Oct 11 02:04 ..
dr-xr-xr-x   2 root     wheel        1024 May  8  1997 bin
dr-xr-xr-x   2 root     wheel        1024 May  8  1997 etc
drwxrwxrwx   2 root     wheel        1024 Aug 21 18:05 incoming
dr-xr-xr-x   2 root     wheel        1024 Nov 17  1993 lib
-rw-r--r--   1 root     root       580368 Oct 11 02:04 ls-lR.gz
drwxr-xr-x  15 4470     102          1024 Sep  7 00:06 mirrors
drwxr-xr-x   2 4461     4460         1024 Sep 20 12:48 packages
drwx--x--x   4 4461     4460         1024 Mar 27  1998 private
drwxr-xr-x   6 4461     4460         1024 Apr 17 12:11 pub
dr-xr-xr-x   3 root     wheel        1024 May  8  1997 usr
-rw-r--r--   1 4461     root          325 Nov 15  1997 welcome.msg
226 Transfer complete.
*/

#define find_space1(s,e) {e=s; while(*e && *e!=' ')e++; if (!*e) return 1;}
#define skip_spaces1(s)  {while(*s && *s==' ')s++;if (!*s) return 1;}

static int parse_unix (char *line, file *f)
{
    char *el_rights, *el_size, *el_date, *el_name;
    char *p, *pa, *pb;

    el_rights = line;
    switch (*el_rights)
    {
    case 'd':      f->flags = FILE_FLAG_DIR; break;
    case '-':      f->flags = FILE_FLAG_REGULAR; break;
    case 'l':      f->flags = FILE_FLAG_LINK; break;
    default:       return 1;
    }
    f->rights = perm_t2b (el_rights) & 07777;
    if (f->rights == 0) f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    pa = line + 10;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give # of hard links */

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give userid */
    *pb = '\0';
    f->owner = strdup (pa);

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give groupid/size */
    *pb = '\0';
    f->group = strdup (pa);
    el_size = pa;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);
    //*pb = 0;
    if (*pa >= '0' && *pa <= '9') // size is here
    {
        el_size = pa;
        pa = pb+1;
        skip_spaces1 (pa);
        find_space1 (pa, pb);
        //*pb = 0;
    }
    else
    {
        free (f->group);
        f->group = NULL;
    }
    el_date = pa;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give day */
    //*pb = 0;
    //el_day = pa;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give time/year */
    //*pb = 0;
    //el_timeyear = pa;

    pb++;
    skip_spaces1 (pb);
    //pb = ??
    el_name = pb;

    // pointers are set now, setting fields in 'file' structure

    f->size = atol (el_size);
    f->mtime = parse_date_time (DATE_FMT_4, el_date, NULL);
    dmsg ("[john] parse_unix: date = [%s], mtime=[%ld]\n", el_date, f->mtime);
    f->name = strdup (el_name);
    if ((p = strstr (f->name, "->")) != NULL) *(p-1) = 0;

    return 0;
}

/* -------------------------------------------------------------
.                0           DIR   04-21-98   23:16  ATW
.                0           DIR   04-21-98   23:16  font
.                0           DIR   08-22-98   21:58  Programs
.                0           DIR   10-22-97   09:01  Stuff
.             3085      A          08-25-98   21:36  bfmpp.c
.             3384      A          01-09-98   19:32  fft.c
.              238      A          10-20-96   14:00  fft.h
.              465      A          07-26-98   22:00  fourier.for
.                9      A          08-29-98   09:13  freq
.             5197      A          08-08-95   05:15  gd.h
.             1910      A          08-21-98   14:16  generate.for
.            36825      A          09-23-94   05:00  undelete.com
.            72048     RA          09-23-94   06:24  unpack.exe
.            77200     RA          09-23-94   06:25  unpack2.exe
.            15870      A          10-10-94   03:19  view.exe
 */
static int parse_ibm (char *line, file *f)
{
    if (strlen(line) < 54) return 1;

    if (memcmp (line+29, "DIR", 3) == 0) f->flags = FILE_FLAG_DIR;
    else                                 f->flags = FILE_FLAG_REGULAR;

    f->size = extract_int64 (line, 0, 18);
    f->mtime = parse_date_time (DATE_FMT_3, line+35, line+46);
    f->name = extract_string (line, 53, 0);
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/* -------------------------------------------------------------
d------r--   0 0     ----              0 Jan 02  1980 ..
d------r--   0 0     ----              0 May 15  1996 MustHave
d------r--   0 0     ----              0 Jun 09  1997 OK
-------r--   0 0     -A--          49887 Sep 16  1995 koi8cour.arj
-------r--   0 0     -A--          57743 Oct 01  1995 PCMTAB.RAR
-------r--   0 0     -A--          25172 Oct 12  1995 webkoi.zip
-------r--   0 0     -A--           1469 Dec 05  1995 exprus.pat
-------r--   0 0     -A--         360714 Jan 19  1996 WARPCYR4.ZIP
-------r--   0 0     -A--          45791 May 30  1996 web11koi.zip
-------r--   0 0     -A--            598 Jun 04  1996 convert.cmd
-------r--   0 0     -A--         104204 Jul 03  1996 netscap2.zip
-------r--   0 0     -A--          35948 Aug 11  1996 tn-1.0.zip
-------r--   0 0     -A--           9843 Aug 19  1996 os2hdd2.zip
-------r--   0 0     -A--            100 Aug 23  1996 dir.msg
-------r--   0 0     -A--          40156 Oct 21  1996 tn-1.0.1.zip
-------r--   0 0     -A--         196362 Dec 21  1996 koi8_os2.RAR
-------r--   0 0     -A--          49890 Dec 22  1996 courier-koi8.zip
-------r--   0 0     -A--         192943 Feb 16  1997 ns_1251.zip
-------r--   0 0     -A--         154599 Feb 16  1997 ns_koi8.zip
-------r--   0 0     -A--           1368 Apr 01  1997 msg.cmd
-------r--   0 0     -A--          69092 Jun 16  1997 dc_os2.zip
-------r--   0 0     -A--           2198 Jul 13  1997 space.rsp
-------r--   0 0     -A--            617 Jul 13  1997 space.scr
-------r--   0 0     -A--          55141 Sep 07  1997 ping-8_1.zip
-------r--   0 0     -A--           3233 Dec 03  1997 pmm_scr.rar
-------r--   0 0     -A--          53248 Mar 02  1998 diunpack.exe
 */

static int parse_neologic (char *line, file *f)
{
    if (strlen(line) < 33) return 1;

    if (line[0] == 'd') f->flags = FILE_FLAG_DIR;
    else                f->flags = FILE_FLAG_REGULAR;

    f->size = extract_int64 (line, 7, 18);
    f->mtime = parse_date_time (DATE_FMT_4, line+19, NULL);

    f->name = extract_string (line, 32, 0);
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/* ------------------------------------------------------------- */

static int parse_oldnt (char *line, file *f)
{
    if (strlen(line) < 33) return 1;

    if (memcmp (line+25, "DIR", 3) == 0) f->flags = FILE_FLAG_DIR;
    else                                 f->flags = FILE_FLAG_REGULAR;

    f->size = extract_int64 (line, 17, 38);
    f->mtime = parse_date_time (DATE_FMT_3, line, line+10);
    f->name = strdup (line+39);
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/* ------------------------------------------------------------- */

static int parse_unisys (char *line, file *f)
{
    char *p, *p1;

    f->flags = FILE_FLAG_REGULAR;

    p = strchr (line, ' ');
    if (p == NULL) return 1;
    *p = '\0';

    f->name = strdup (line);

    // skip whitespace after filename
    p++;
    while (*p == ' ') p++;

    // skip DATA/FTPDATA
    p1 = strchr (p, ' ');
    if (p1 == NULL) return 1;
    *p1 = '\0';
    if (strcmp (p, "DATA") != 0 && strcmp (p, "FTPDATA") != 0) return 1;

    // skip whitespace after DATA/FTPDATA
    p = p1 + 1;
    while (*p == ' ') p++;

    // get size
    p1 = strchr (p, ' ');
    if (p1 == NULL) return 1;
    *p1 = '\0';
    f->size = atol (p);

    // skip whitespace after size
    p = p1 + 1;
    while (*p == ' ') p++;

    // get date
    p1 = strchr (p, ' ');
    if (p1 == NULL) return 1;
    *p1 = '\0';
    if (strlen (p) != 8) return 1;

    f->mtime = parse_date_time (DATE_FMT_3, p, p1+1);
    f->rights = 0644;

    return 0;
}

/* -------------------------------------------------------------
-r--r--r--           0       49       49 Sep  5 15:42 vid_quota1.ram
-r--r--r--           0       53       53 Sep  5 15:42 vid_quota2-110.ram
-r--r--r--           0       64       64 Sep  5 15:42 vid_quota2-20.ram
-r--r--r--           0       64       64 Sep  5 15:42 vid_quota2-45.ram
-r--r--r--           0       52       52 Sep  5 15:42 vid_quota2-75.ram
-r--r--r--           0     1895     1895 Mar 17 09:45 vid_shag-110.html
-r--r--r--           0     1892     1892 Mar 17 09:38 vid_shag-15.html
-r--r--r--           0     1737     1737 Mar 17 09:38 vid_shag-20.html
-r--r--r--           0     1677     1677 Mar 17 18:59 VOTEforSHAG.html
-r--r--r--           0      412      412 Mar 17 18:59 VoteWriteIn.html
dr--r--r--               folder       14 Apr 17 14:52 wav
226 Transfer complete
remote: shag
8159 bytes received in 9,03 seconds (0 Kbytes/s)
ftp> quote syst
215 MACOS Peter's Server
ftp> dir
200 PORT command successful
150 Opening connection
dr--r--r--               folder        0 Sep 10 08:41 shag
-r--r--r--           0    11165    11165 Mar 17 18:56 SHAGDates.html
226 Transfer complete
130 bytes received in 0,98 seconds (0 Kbytes/s)
ftp>
*/
static int parse_macos_peter (char *line, file *f)
{
    char *el_rights, *el_size, *el_date, *el_name;
    char *p, *pa, *pb;

    el_rights = line;
    switch (*el_rights)
    {
    case 'd':      f->flags = FILE_FLAG_DIR; break;
    case '-':      f->flags = FILE_FLAG_REGULAR; break;
    default:       return 1;
    }

    pa = line + 10;

    if (f->flags & FILE_FLAG_REGULAR)
    {
        skip_spaces1 (pa);
        find_space1 (pa, pb);      /* now pa, pb give # of hard links */
        pa = pb+1;
    }

    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give size (instead of userid) */

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give size */
    *pb = 0;
    el_size = pa;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give month */
    //*pb = 0;
    el_date = pa;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give day */
    //*pb = 0;
    //el_day = pa;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give time/year */
    //*pb = 0;
    //el_timeyear = pa;

    pb++;
    skip_spaces1 (pb);
    el_name = pb;

    // pointers are set now, setting fields in 'file' structure
    f->size = atol (el_size);
    f->mtime = parse_date_time (DATE_FMT_4, el_date, NULL);
    f->name = strdup (el_name);
    if ((p = strstr (f->name, "->")) != NULL) *(p-1) = 0;
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/* -------------------------------------------------------------
FUELOEP         76350 03/09/97 08:53:48 *STMF      home/fueloep/v4r1over.pre
 */

static int parse_as400 (int nsite, char *line, file *f)
{
    if (strlen(line) < 52) return 1;

    if (memcmp (line+40, "*DIR", 4)  == 0) f->flags = FILE_FLAG_DIR;
    else if (memcmp (line+40, "*DDIR", 5) == 0) f->flags = FILE_FLAG_DIR;
    else if (memcmp (line+40, "*FLR", 4)  == 0) f->flags = FILE_FLAG_DIR;
    else if (memcmp (line+40, "*LIB", 4)  == 0) f->flags = FILE_FLAG_DIR;
    else if (memcmp (line+40, "*STMF", 5) == 0) f->flags = FILE_FLAG_REGULAR;
    else if (memcmp (line+40, "*MEM", 4)  == 0) f->flags = FILE_FLAG_REGULAR;
    else if (memcmp (line+40, "*DOC", 4)  == 0) f->flags = FILE_FLAG_REGULAR;
    else if (memcmp (line+40, "*FILE", 5) == 0) f->flags = FILE_FLAG_REGULAR;
    else return 1;

    // check for YMD
    if (line[22] == '9') site[nsite].date_fmt = DATE_FMT_5;

    f->size = extract_int64 (line, 10, 21);
    if (memcmp (line+40, "*MEM", 4) == 0)
        f->mtime = parse_date_time (site[nsite].date_fmt, line+22, NULL);
    else
        f->mtime = parse_date_time (site[nsite].date_fmt, line+22, line+31);

    // parse filename
    f->name = extract_string (line, 51, 0);
    str_strip (f->name, " /\n\r");
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/* -------------------------------------------------------------
MOTD.TXT;7               1/9           3-DEC-1996 12:50:42  [SYSTEM]               (,,,R)
XVTEXT.C;1              94/94         12-JUN-1995 13:23:06  [SYSTEM]
AAAREADME.V3;1        1425/1426       27-SEP-1996 11:50:49  [SYSTEM]
*/

static char *leftover_name = NULL;

static int parse_vms (char *line, file *f)
{
    char *p, *p1, *p2;

    if (strncmp (line, "Directory ", 10) == 0) return 1;
    if (strncmp (line, "Total of ", 9) == 0) return 1;

    if (strlen(line) < 50)
    {
        leftover_name = strdup (line);
    }

    // find where name is
    if (line[0] == ' ')
    {
        if (leftover_name == NULL) return 1;
        f->name = leftover_name;
        leftover_name = NULL;
        p = line;
    }
    else
    {
        p = strchr (line, ' ');
        if (p == NULL || p-line > 26) return 1;
        *p = '\0';
        f->name = strdup (line);
        leftover_name = NULL;
    }

    // check whether this is a directory or a file
    f->flags = FILE_FLAG_REGULAR;
    p2 = strstr (f->name, ".DIR;");
    if (p2 != NULL)
    {
        f->flags = FILE_FLAG_DIR;
        *p2 = '\0';
    }

    // remove version number
    if (options.vms_remove_version)
    {
        p1 = strstr (f->name, ";1");
        if (p1 != NULL) *p1 = '\0';
    }

    // size
    line[26] = '\0';
    f->size = atol (p+1) * 512;

    // timestamp
    f->mtime = parse_date_time (DATE_FMT_1, line+38, line+50);
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/*
FILENAME  CODE  ------------LOGICAL RECORD-----------  ----SPACE----                  SIZE  TYP        EOF      LIMIT R/B  SECTORS #X MX
DB000     PRIV    128W  FB           6          6   1       16  1  1
MXBIGRIO1         128B  FAR       8193       8192   3     4128  1  8

Connected to jazz.external.hp.com.
220 HP ARPA FTP Server [A0009001] (C) Hewlett-Packard Co. 1990
Name (jazz.external.hp.com): anonymous
331 Guest login ok, send ident as password.
Password: ..............
230 User logged on
ftp> dir
200 PORT command ok.
150 File: LISTFILE ./@,2 opened; data connection will be opened
 PATH= /FTPGUEST/PUB/

 CODE  ------------LOGICAL RECORD-----------  ----SPACE----  FILENAME
         SIZE  TYP        EOF      LIMIT R/B  SECTORS #X MX

          16W  HBD          0   67107839   1       32  1  *  incoming/
           1B  BA         115 2147483647   1       16  1  *  testfile


226 Transfer complete.
306 bytes received in 0,41 seconds (0 Kbytes/s)
ftp>

*/

static int parse_mpeix (char *line, file *f)
{
    fly_error ("MPE/ix yet unsupported");

    return 1;
}

/*
HTTP/1.0 200 Gatewaying
Date: Sat, 20 Feb 1999 08:35:56 GMT
MIME-Version: 1.0
Server: Squid 1.1.22
Content-Type: text/html

<!-- HTML listing generated by Squid 1.1.22 -->
<!-- Sat, 20 Feb 1999 08:35:56 GMT -->
<HTML><HEAD><TITLE>
FTP Directory: ftp://crydee.sai.msu.ru/
</TITLE>
</HEAD><BODY>
<H2>
FTP Directory: ftp://crydee.sai.msu.ru/
</H2>
<PRE>
<IMG BORDER=0 SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="../">Parent Directory</A>
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="bin/">bin</A>. . . . . . . . . . . . . . .  [May  8  1997]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="etc/">etc</A>. . . . . . . . . . . . . . .  [Jan 25 19:40]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="incoming/">incoming</A> . . . . . . . . . . . .  [Feb  9 19:47]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="lib/">lib</A>. . . . . . . . . . . . . . .  [Nov 17  1993]
<IMG SRC="internal-gopher-binary" ALT="[FILE]"> <A HREF="ls-lR.gz">ls-lR.gz</A> . . . . . . . . . . . .  [Feb 20 03:07]   1141k
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="mirrors/">mirrors</A>. . . . . . . . . . . . .  [Jan  8 20:49]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="packages/">packages</A> . . . . . . . . . . . .  [Sep 20 12:48]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="private/">private</A>. . . . . . . . . . . . .  [Feb 10 13:54]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . .  [Nov  3 20:45]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="usr/">usr</A>. . . . . . . . . . . . . . .  [May  8  1997]
<IMG SRC="internal-gopher-menu" ALT="[DIR] "> <A HREF="zeus/">zeus</A> . . . . . . . . . . . . . .  [Jan  2 19:33]
</PRE>
<HR>
<ADDRESS>
Generated Sat, 20 Feb 1999 08:35:56 GMT, by ftpget/1.1.22@zeus.sai.msu.su
</ADDRESS></BODY></HTML>

Squid 2.1:

<A HREF="%2e%2e/"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-dirup.gif" ALT="[DIRUP]"></A> <A HREF="%2e%2e/">Parent Directory</A> (<A HREF="%2f/">Root Directory</A>)
<A HREF="bin/"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-dir.gif" ALT="[DIR] "></A> <A HREF="bin/">bin</A>. . . . . . . . . . . . . . . Dec 28  1996
<A HREF="etc/"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-dir.gif" ALT="[DIR] "></A> <A HREF="etc/">etc</A>. . . . . . . . . . . . . . . Oct 24 14:54
<A HREF="lib/"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-dir.gif" ALT="[DIR] "></A> <A HREF="lib/">lib</A>. . . . . . . . . . . . . . . Oct 24 14:41
<A HREF="ls-lR.gz"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-unknown.gif" ALT="[FILE]"></A> <A HREF="ls-lR.gz">ls-lR.gz</A> . . . . . . . . . . . . Feb 26 23:16    548k <A HREF="ls-lR.gz;type=i"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-box.gif" ALT="[DOWNLOAD]"></A>
<A HREF="pub/"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-dir.gif" ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
<A HREF="its_better_to_be_single_than_a_poor_mans_wife"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-unknown.gif" ALT="[FILE]"></A> <A HREF="its_better_to_be_single_than_a_poor_mans_wife">its_better_to_be_single_than_a_></A> Oct 20  1992      1k <A HREF="its_better_to_be_single_than_a_poor_mans_wife;type=a"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-text.gif" ALT="[VIEW]"></A> <A HREF="its_better_to_be_single_than_a_poor_mans_wife;type=i"><IMG BORDER=0 SRC="http://crydee.sai.msu.ru:3128/squid-internal-static/icons/anthony-box.gif" ALT="[DOWNLOAD]"></A>

*/

static int parse_squid (char *line, file *f)
{
    char *p, *p1;
    int  n;

    // try to find the link
    p = strstr (line, " ALT=\"[");
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                           p
    if (p == NULL || strlen (p) < 11) return 1;

    p += 6;
    if      (strncmp (p, "[DIR]",     5) == 0) f->flags = FILE_FLAG_DIR;
    else if (strncmp (p, "[FILE]",    6) == 0) f->flags = FILE_FLAG_REGULAR;
    else if (strncmp (p, "[LINK]",    6) == 0) f->flags = FILE_FLAG_LINK;
    else return 1;

    p  = strstr (p, "A HREF=\"");   if (p == NULL) return 1;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                              p
    p += 8;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                                      p
    p1 = strchr (p, '"');           if (p1 == NULL) return 1;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                                          p1
    *p1 = '\0';
    if (*(p1-1) == '/') *(p1-1) = '\0';
    f->name = strdup (p);

    // timestamp [Sep 20 12:48] [Nov 17  1993]
    p = p1 + 1;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                                           p
    p = strstr (p, "</A>");        if (p == NULL) return 1;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                                               p
    p += 4;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                                                   p
    n = strcspn (p, "JFMAISOND");
    if (n == strlen (p)) return 1;
    p += n;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33
    //                                                                                                 p
    if (strlen (p) < 12) return 1;
    f->mtime = parse_date_time (DATE_FMT_4, p, NULL);

    // size
    p += 12;
    // <A HREF="pub/"><IMG blurb ALT="[DIR] "></A> <A HREF="pub/">pub</A>. . . . . . . . . . . . . . . Jan  2 19:33    548k <A HREF="ls-lR.gz;type=i"><IMG BORDER=0 blurb ALT="[DOWNLOAD]"></A>
    //                                                                                                              p
    f->size = 0;
    if (*p == ']') p++;
    while (*p && *p == ' ') p++;
    if (p == '\0' || *p == '<') return 0; // directories have no size!
    p1 = strchr (p, ' ');
    if (p1 != NULL) *p1 = '\0';
    f->size = atol (p);
    if (str_tailcmp (p, "k") == 0) f->size *= 1024;
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/*
 Netscape proxy server
<TITLE>Directory of /</TITLE>
 <H2>Current directory is /</H2>
 <PRE> <A HREF="/bin/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> bin/</A>                              Tue Apr 27 16:22:00 1999 Directory
 <A HREF="/etc/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> etc/</A>                              Tue Apr 27 16:32:00 1999 Directory
 <A HREF="/incoming/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> incoming/</A>                         Wed Jul 14 15:56:00 1999 Directory
 <A HREF="/ls-lR.gz"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-unknown"> ls-lR.gz</A>               1227 Kb    Sat Jul 17 02:08:00 1999
 <A HREF="/mirrors/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> mirrors/</A>                          Wed Jun 16 15:15:00 1999 Directory
 <A HREF="/packages/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> packages/</A>                         Mon May 03 09:18:00 1999 Directory
 <A HREF="/private/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> private/</A>                          Sat Jun 26 14:57:00 1999 Directory
 <A HREF="/pub/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> pub/</A>                              Sat Jun 12 12:36:00 1999 Directory
 <A HREF="/zeus/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> zeus/</A>                             Sat Jan 02 00:00:00 1999 Directory
 </PRE>
 <PRE><A HREF="/">Up to higher level directory</A><BR> <A HREF="/pub/.1/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> .1/</A>                               Wed Jun 16 15:15:00 1999 Directory
 <A HREF="/pub/.2/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> .2/</A>                               Sat Jun 12 12:46:00 1999 Directory
 <A HREF="/pub/astronomy"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> astronomy</A>                         Sat Jun 12 12:36:00 1999 Symbolic link
 <A HREF="/pub/comp/"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> comp/</A>                             Sat Jun 12 12:36:00 1999 Directory
 <A HREF="/pub/local"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> local</A>                             Sat May  1 17:21:00 1999 Symbolic link
 <A HREF="/pub/rec"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-menu"> rec</A>                               Sat May  1 17:07:00 1999 Symbolic link

 */

static int parse_nsproxy (char *line, file *f)
{
    char *p, *p1, *p2;

    // remove spaces
    str_strip2 (line, " ");

    // skip stupid "Up to higher level directory"
    p = strstr (line, "Up to higher level directory");
    if (p != NULL)
    {
        p = strstr (p, "<BR>");
        if (p == NULL) return 1;
        line = p + 4;
    }

    // determine type
    if      (str_tailcmp (line, "Directory") == 0)     f->flags = FILE_FLAG_DIR;
    else if (str_tailcmp (line, "Symbolic link") == 0) f->flags = FILE_FLAG_LINK;
    else                                               f->flags = FILE_FLAG_REGULAR;

    // try to find the link
    p = strstr (line, "<A HREF=\"");
    if (p == NULL || strlen (p) < 11) return 1;
    p += 9;
    //<A HREF="/ls-lR.gz"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-unknown"> ls-lR.gz</A>               1227 Kb    Sat Jul 17 02:08:00 1999
    //         p
    p1 = strchr (p, '"');
    if (p1 == NULL) return 1;
    *p1 = '\0';
    str_strip2 (p, "/");
    p2 = strrchr (p, '/');
    if (p2 == NULL) p2 = p-1;
    f->name = strdup (p2+1);

    //<A HREF="/ls-lR.gz"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-unknown"> ls-lR.gz</A>               1227 Kb    Sat Jul 17 02:08:00 1999
    p = p1 + 1;
    p = strstr (p, "</A>");        if (p == NULL) return 1;
    //<A HREF="/ls-lR.gz"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-unknown"> ls-lR.gz</A>               1227 Kb    Sat Jul 17 02:08:00 1999
    //                                                                                         p
    p += 4;
    //<A HREF="/ls-lR.gz"><IMG ALIGN=absbottom BORDER=0 SRC="internal-gopher-unknown"> ls-lR.gz</A>               1227 Kb    Sat Jul 17 02:08:00 1999
    //                                                                                             p
    while (*p && *p==' ') p++;
    if (!*p) return 1;

    //ls-lR.gz</A>               1227 Kb    Sat Jul 17 02:08:00 1999
    //incoming/</A>                         Wed Jul 14 15:56:00 1999 Directory
    f->size = 0;
    if (f->flags & FILE_FLAG_REGULAR)
    {
        p1 = strchr (p, ' ');
        if (p1 == NULL) return 1;
        *p1 = '\0';
        f->size = atol (p);
        if (strnicmp1 (p1+1, "kb", 2) == 0) f->size *= 1024;
        if (strnicmp1 (p1+1, "mb", 2) == 0) f->size *= 1024*1024;
        p = p1+3;
    }

    // Sat Jul 17 02:08:00 1999
    // Wed Jul 14 15:56:00 1999 Directory
    while (*p && *p==' ') p++;
    if (!*p) return 1;

    if (strlen (p) < 24) return 1;
    p += 4;
    f->mtime = parse_date_time (DATE_FMT_6, p, NULL);

    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/*
<HEAD>
<title>FTP Menu at ftp.ayukov.com</title>
</HEAD>
<h1>FTP Menu</h1>
<HR>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/00-index">
<IMG ALT="[TXT]" SRC="http://10.7.2.40:80/-http-gw-internal-/text.gif"></A>        00-index <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.53-1.i386.rpm">
<IMG ALT="[TXT]" SRC="http://10.7.2.40:80/-http-gw-internal-/blank.gif"></A>        nftp-1.53-1.i386.rpm <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.53-linuxlibc5-x86.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.53-linuxlibc5-x86.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.53-linuxlibc6-x86.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.53-linuxlibc6-x86.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-1.i386.rpm">
<IMG ALT="[TXT]" SRC="http://10.7.2.40:80/-http-gw-internal-/blank.gif"></A>        nftp-1.60-1.i386.rpm <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-alpha-linuxlibc6.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-alpha-linuxlibc6.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-alpha-osf4.tar.Z">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-alpha-osf4.tar.Z <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-beos4.zip">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp-1.60-i386-beos4.zip <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-freebsd2.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-i386-freebsd2.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-freebsd3.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-i386-freebsd3.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-linuxlibc5.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-i386-linuxlibc5.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-linuxlibc6.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-i386-linuxlibc6.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-os2.zip">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp-1.60-i386-os2.zip <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-solaris.tar.Z">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-i386-solaris.tar.Z <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-win32.exe">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-i386-win32.exe <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-i386-win32.zip">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp-1.60-i386-win32.zip <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-rs6000-aix4.tar.Z">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-rs6000-aix4.tar.Z <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-sparc-linuxlibc6.tar.gz">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-sparc-linuxlibc6.tar.gz <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp-1.60-sparc-solaris.tar.Z">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp-1.60-sparc-solaris.tar.Z <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp153o.zip">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp153o.zip <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp153w.exe">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp153w.exe <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp160o.zip">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp160o.zip <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp160w.exe">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/binary.gif"></A>        nftp160w.exe <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/nftp160w.zip">
<IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp160w.zip <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/readme.LIBC-WARNING">
<IMG ALT="[TXT]" SRC="http://10.7.2.40:80/-http-gw-internal-/blank.gif"></A>        readme.LIBC-WARNING <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/readme.OLD-VERSIONS">
<IMG ALT="[TXT]" SRC="http://10.7.2.40:80/-http-gw-internal-/blank.gif"></A>        readme.OLD-VERSIONS <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/support/">
<IMG ALT="[DIR]" SRC="http://10.7.2.40:80/-http-gw-internal-/menu.gif"></A>        support/ <br>
<A HREF="ftp://ftp.ayukov.com/pub/nftp/tools/">
<IMG ALT="[DIR]" SRC="http://10.7.2.40:80/-http-gw-internal-/menu.gif"></A>        tools/ <br>
<br><hr>
<A HREF="http://10.7.2.40:80/http://-internal-/-http-gw-internal-/version.html">http-gw version  4.1 / 2</A>
 (10.7.2.40)]
*/

static int parse_bordermanager (char *line, file *f)
{
    char     *p, *p1;
    time_t   now = time (NULL);

    // try to find the link
    p = strstr (line, " ALT=\"[");
    // <IMG ALT="[BIN]" SRC="http://10.7.2.40:80/-http-gw-internal-/dosbin.gif"></A>        nftp160w.zip <br>
    //     p
    if (p == NULL || strlen (p) < 11) return 1;

    p += 6;
    if      (strncmp (p, "[DIR]",  5) == 0) f->flags = FILE_FLAG_DIR;
    else if (strncmp (p, "[TXT]",  5) == 0) f->flags = FILE_FLAG_REGULAR;
    else if (strncmp (p, "[BIN]",  5) == 0) f->flags = FILE_FLAG_REGULAR;
    else if (strncmp (p, "[LINK]", 6) == 0) f->flags = FILE_FLAG_LINK;
    else return 1;

    p  = strstr (p, "></A>");   if (p == NULL) return 1;
    // ...sbin.gif"></A>        nftp160w.zip <br>
    //             p
    p += 5;
    // ...sbin.gif"></A>        nftp160w.zip <br>
    //                  p
    while (*p && *p == ' ') p++;
    if (!*p) return 1;
    // ...sbin.gif"></A>        nftp160w.zip <br>
    //                          p
    p1 = strstr (p, "<br>");
    if (p1 == NULL) return 1;
    // ...sbin.gif"></A>        nftp160w.zip <br>
    //                          p            p1
    *p1 = '\0';
    str_strip (p, " /");
    f->name  = strdup (p);
    f->mtime = now;
    f->size = 0;
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/*
type=cdir;modify=19990426150228;perm=el; /MISC/src
type=pdir;modify=19990426150227;perm=el; /MISC
type=dir;modify=19990426150228;perm=el; CVS
type=dir;modify=19990426150228;perm=el; INSTALL
type=dir;modify=19990426150230;perm=el; INSTALLI
type=dir;modify=19990426150230;perm=el; TREES

size       -- Size in octets
modify     -- Last modification time
create     -- Creation time
type       -- Entry type
unique     -- Unique id of file/directory
perm       -- File permissions, whether read, write, execute is allowed for the login id.
lang       -- Language of the filename per IANA[12] registry.
media-type -- MIME media-type of file contents per IANA registry.
charset    -- Character set per IANA registry (if not UTF-8)
 */

static int parse_mlst (char *line, file *f)
{
    char *filename, *fact, *p, *p1, *d;

    // find the delimiting space
    filename = strchr (line, ' ');
    if (filename == NULL) return 1;
    *filename++ = '\0';

    // now get every fact
    p = line;
    f->size   = 0;
    f->rights = 0;
    while ((p1 = strchr (p, ';')) != NULL)
    {
        *p1 = '\0';
        fact = p;
        p = p1 + 1;
        // get the fact
        d = strchr (fact, '=');
        if (d == NULL) continue;
        *d++ = '\0';
        str_lower (fact);
        if      (strcmp (fact, "size") == 0)
        {
            f->size = atol (d);
        }
        else if (strcmp (fact, "modify") == 0)
        {
            // pair = Moscow - 8
            f->mtime = gm2local (parse_date_time (DATE_FMT_7, d, NULL));
        }
        else if (strcmp (fact, "type") == 0)
        {
            str_lower (d);
            if (strcmp (d, "file") == 0)
            {
                f->flags = FILE_FLAG_REGULAR;
            }
            else if (strcmp (d, "dir") == 0)
            {
                f->flags = FILE_FLAG_DIR;
            }
            else if (strcmp (d, "pdir") == 0)
            {
                f->flags = FILE_FLAG_DIR;
                filename = "..";
            }
            else
            {
                return 1;
            }
        }
        else if (strcmp (fact, "unix.mode") == 0)
        {
            f->rights = strtol (d, NULL, 8) & 0777;
        }
        /*
        else if (strcmp (fact, "create") == 0)
        {
            // ignore it
        }
        else if (strcmp (fact, "unique") == 0)
        {
            // ignore it
        }
        else if (strcmp (fact, "perm") == 0)
        {
            // ignore it
        }
        else if (strcmp (fact, "lang") == 0)
        {
            // ignore it
        }
        else if (strcmp (fact, "media-type") == 0)
        {
            // ignore it
        }
        else if (strcmp (fact, "charset") == 0)
        {
            // ignore it
        }
        */
    }

    if (f->rights == 0) f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;
    f->name  = strdup (filename);

    return 0;
}

/*
MVS:
unbmvs1.csd.unb.ca

VM:

ksuvm.ksu.edu
220-FTPSERVE IBM VM V2R4 at VM.KSU.EDU, 05:45:06 CST MONDAY 12/20/99
215-VM/ESA Version 2 Release 2.0, service level 9903
    VM/CMS Level 13, Service Level 903
215 VM is the operating system of this server.]

nervm.nerdc.ufl.edu
FTPSERVE IBM VM V2R3 at nervm.nerdc.ufl.edu, 06:42:00 EST MONDAY
VM is the operating system of this server

$README  $FIRST   F         80          7          1 11/21/97 23:26:08 FTP301
ART               DIR        -          -          - 11/24/95 17:45:47 -
AS                DIR        -          -          - 10/18/98 18:43:16 -
BLAH              DIR        -          -          -  2/07/98  1:29:15 -
CAIRO             DIR        -          -          -  8/06/96 11:12:52 -
FAFS              DIR        -          -          -  5/29/98 10:20:58 -
GEOPLAN           DIR        -          -          -  7/22/97 20:17:00 -
HWPW              DIR        -          -          -  4/04/96 21:46:25 -
JUNK              DIR        -          -          -  7/28/96  0:35:11 -
MODS              DIR        -          -          - 11/24/95 17:46:06 -
OUTGOING          DIR        -          -          - 10/19/96 20:32:04 -
RPENA             DIR        -          -          - 10/19/98  8:25:53 -
SAWFLY            DIR        -          -          -  6/18/97 21:10:21 -
TEST              DIR        -          -          -  7/25/96 18:20:58 -
01234567890123456
*/

static int parse_vmsp (char *line, file *f)
{
    char filename[32];

    if (strlen (line) < 71) return 1;

    if   (memcmp (line+18, "DIR",  3) == 0) f->flags = FILE_FLAG_DIR;
    else                                    f->flags = FILE_FLAG_REGULAR;

    line[8]  = '\0';
    line[17] = '\0';

    strcpy (filename, line);
    str_strip (filename, " ");
    if (line[9] != ' ')
    {
        strcat (filename, ".");
        strcat (filename, line+9);
        str_strip (filename, " ");
    }
    f->name  = strdup (filename);

    line[61] = '\0';
    line[70] = '\0';
    f->mtime = parse_date_time (DATE_FMT_3, line+53, line+62);

    line[30] = '\0';
    line[41] = '\0';
    f->size = atoi (line+21) * atoi (line+31);
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/*
 215 NETWARE Type: L8

d [RWCE-FM-] dex                               512 Jan 02 15:48 adaptec easy cd creator v.4.0 deluxe-pros
d [RWCE-FM-] dex                               512 Jan 02 16:47 adaptec_easy_cd_creator_deluxe_3.01a
d [RWCE-FM-] dex                               512 Jan 02 17:26 ahead nero 4.0.56
d [RWCE-FM-] dex                               512 Jan 02 17:26 cdrwin 3.7b
d [RWCE-FM-] dex                               512 Jan 02 17:30 fireburner v0.92e
d [RWCE-FM-] dex                               512 Jan 02 17:35 psxcopy v5.0 -registered
- [RWCE-FM-] dex                            514405 Jan 02 17:27 cdr37b-e.exe
d [RWCE-FM-] dex                               512 Jan 02 17:26 crack
- [R----F--] maarten                      647815168 Dec 30  1999 3.4-install.iso
- [R----F--] maarten                        831479 Jan 03 20:45 combprog.exe
d [R----F--] maarten                           512 Jan 02 00:34 redhat62

*/

static int parse_netware (char *line, file *f)
{
    char *pa, *pb;

    if (strlen (line) < 14) return 1;

    switch (line[0])
    {
    case 'd':      f->flags = FILE_FLAG_DIR; break;
    case '-':      f->flags = FILE_FLAG_REGULAR; break;
    default:       return 1;
    }

    pa = line + 1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give Netware access rights */
    *pb = '\0';
    str_strip2 (pa, "[]");
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give userid */

    pa = pb+1;
    skip_spaces1 (pa);
    find_space1 (pa, pb);      /* now pa, pb give size */
    *pb = 0;
    f->size = atol (pa);

    pa = pb+1;                 // here we have 12 bytes of timestamp
    if (strlen (pa) < 15) return 1;

    f->mtime = parse_date_time (DATE_FMT_4, pa, NULL);
    f->name = strdup (pa+13);
    return 0;

}

/*
[C:\]ftp 10.38.10.1
IBM TCP/IP til OS/2 - FTP-klient (18:09:38 on Jun 18 1996)
Forbundet med 10.38.10.1.
220-TCPFTP IBM MVS V3R2 at DKTOPM01, 12:11:02 on 2000/01/20
220 Connection will close if idle for more than 5 minutes.
Navn (10.38.10.1): ksj
331 Send password please.
Kodeord: .......
230 KSJ is logged on.  Working directory is "KSJ.".
ftp> quote syst
215 MVS is the operating system of this server.
ftp> dir
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
SYSPP1 3390   2000/01/20  1    3  FB      80  3120  PO  BSCSYS.ISPTABL
Migrated                                                BSCSYS.ISPUSER
SYSPP2 3390   1999/12/07  1    5  FB      80  4240  PO  BSCSYS.SAS
SYSPP5 3390                                        VSAM DDIR
SYSPP5 3390                                        VSAM DDIR.D
SYSPP5 3390                                        VSAM DDIR.I
SYSPP1 3390   2000/01/20  3   11  FB      80  3120  PO  ISPF.PROFILE
Migrated                                                LOG.MISC
Migrated                                                RMFOS130.ADMGDF
Migrated                                                RMFOS130.ISPTABLE
SYSPP1 3390   2000/01/14  1   90  FB      80 27920  PO  SMPEJCL.DATA
SYSPP5 3390   2000/01/18  1    1  VA     125   129  PS  SPFLOG0.LIST
SYSPP5 3390   2000/01/20  1    1  VA     125   129  PS  SPFLOG1.LIST
SYSPP5 3390   2000/01/06  2    2  VA     125   129  PS  SPFLOG3.LIST
SYSPP5 3390   2000/01/07  1    1  VA     125   129  PS  SPFLOG4.LIST
SYSPP5 3390   2000/01/10  1    1  VA     125   129  PS  SPFLOG5.LIST
SYSPP5 3390   2000/01/11  1    1  VA     125   129  PS  SPFLOG6.LIST
SYSPP5 3390   2000/01/12  1    1  VA     125   129  PS  SPFLOG7.LIST
SYSPP4 3390   2000/01/14  1    1  VA     125   129  PS  SPFLOG8.LIST
SYSPP1 3390   2000/01/17  1    1  VA     125   129  PS  SPFLOG9.LIST
SYSPP1 3390   2000/01/14  1    1  FBA    133 13566  PS  SUPERC.LIST
SYSPP2 3390   2000/01/06  1    3  FBA     80  3120  PO  TUP.HDS
SYSPP4 3390   2000/01/20  1   90  FB      80 27920  PO  TUP.JCL
SYSPP4 3390   2000/01/06  1   90  FB      80 27920  PO  TUP.JCL.ATGEMME
SYSPP5 3390   2000/01/18  1   30  U     9076  9076  PO  TUP.LOD
R5TE11 3390   2000/01/06  1   15  FB      80 27920  PO  TUP.PAN
SYSPP4 3390   2000/01/20  2   75  FB      80 27920  PO  TUP.REX
SYSPP5 3390   2000/01/20  1  148  FB      80 27920  PO  TUP.SRC
SYSPP2 3390   2000/01/06  1   15  FBA     80  3120  PS  TUP.SYSOUT
250 List completed successfully.
2051 byte modtaget p† 5,56 sekunder (0 KB/sek.)
ftp> dir 'ksj.tup.s*'
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
SYSPP5 3390   2000/01/20  1  148  FB      80 27920  PO  'KSJ.TUP.SRC'
SYSPP2 3390   2000/01/06  1   15  FBA     80  3120  PS  'KSJ.TUP.SYSOUT'
250 List completed successfully.
ekstern: 'ksj.tup.s*'
210 byte modtaget p† 0,19 sekunder (1 KB/sek.)
ftp> dir 'sys1.vvds.vsy*'
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
SYSPP1 3390                                        VSAM 'SYS1.VVDS.VSYSPP1'
SYSPP4 3390                                        VSAM 'SYS1.VVDS.VSYSPP4'
SYSPP6 3390                                        VSAM 'SYS1.VVDS.VSYSPP6'
SY1CAT 3390                                        VSAM 'SYS1.VVDS.VSY1CAT'
SY1RES 3390                                        VSAM 'SYS1.VVDS.VSY1RES'
SY3PP1 3390                                        VSAM 'SYS1.VVDS.VSY3PP1'
250 List completed successfully.
ekstern: 'sys1.vvds.vsy*'
527 byte modtaget p† 0,06 sekunder (8 KB/sek.)
ftp> dir 'sys5.vs*'
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
SYSPP6 3390   1997/01/08  1    3  FB      80  7200  PO  'SYS5.VSAMD.DATA'
Migrated                                                'SYS5.VSAMDATA'
250 List completed successfully.
ekstern: 'sys5.vs*'
213 byte modtaget p† 0,18 sekunder (1 KB/sek.)
ftp> dir 'sys1.jes3*'
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
SY1RES 3390   2000/01/20  1   12  FB      80 27920  PO  'SYS1.JES3.ISPCLIB'
SY1RES 3390   2000/01/20  1    5  FB      80 27920  PO  'SYS1.JES3.ISPMLIB'
SY1RES 3390   2000/01/20  1   12  FB      80 27920  PO  'SYS1.JES3.ISPPLIB'
SY1RES 3390   2000/01/20  1    5  FB      80 27920  PO  'SYS1.JES3.ISPTLIB'
Error determining attributes                            'SYS1.JES3CKPT'
Error determining attributes                            'SYS1.JES3CKP2'
Error determining attributes                            'SYS1.JES3JCT'
SY1RES 3390   2000/01/20  4  179  U    32760 32760  PO  'SYS1.JES3LIB'
SY1RES 3390   2000/01/17  2  370  FB      80 27920  PO  'SYS1.JES3MAC'
SYSPP7 3390   2000/01/07  2   45  VBA    125   129  PS  'SYS1.JES3OUT'
Error determining attributes                            'SYS1.JES3SNAP'
SY1RES 3390   2000/01/13  2 2286  FB      80 27920  PO  'SYS1.JES3SRC'
250 List completed successfully.
ekstern: 'sys1.jes3*'
952 byte modtaget p† 1,40 sekunder (0 KB/sek.)
ftp> dir 'sys1.haspace'
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
SYSPP7 3390   ***NONE***  1    2  F     4096  4096  PS  'SYS1.HASPACE'
250 List completed successfully.
ekstern: 'sys1.haspace'
137 byte modtaget p† 0,09 sekunder (1 KB/sek.)
ftp> dir 'sys1.ahelp'
200 Port request OK.
125 List started OK.
Volume Unit    Referred Ext Used Recfm Lrecl BlkSz Dsorg Dsname
OS3DLB Not Mounted                                      'SYS1.AHELP'
250 List completed successfully.
ekstern: 'sys1.ahelp'
135 byte modtaget p† 0,09 sekunder (1 KB/sek.)
ftp>
*/

static int parse_mvs1 (char *line, file *f)
{
    int64_t blksz, used;

    dmsg ("MVS1 parser\n");

    if (strlen(line) < 57) return 1;

    if (options.os390_hide &&
        memcmp (line+52, "PS", 2) &&
        memcmp (line+52, "PO", 2) &&
        memcmp (line+52, "  ", 2)) return 1;

    if (str_headcmp (line, "Volume Unit    Referred") == 0) return 1;

    // type: DsOrg=PO - directory
    if (memcmp (line+52, "PO", 2) == 0)
        f->flags = FILE_FLAG_DIR;
    else
        f->flags = FILE_FLAG_REGULAR;

    blksz = extract_int64 (line, 45, 50);
    used  = extract_int64 (line, 27, 32);
    f->size = blksz * used;
    //dmsg ("lrecl = %ld, used = %ld\n", lrecl, used);
    //f->size  = extract_int64 (line, 40, 44);

    f->lrecl = extract_long (line, 38, 44);

    line[24] = '\0';
    dmsg ("date: %s\n", line+14);
    if (strspn (line+14, "0123456789/") == 10)
        f->mtime = parse_date_time (DATE_FMT_8, line+14, NULL);
    else
        f->mtime = 0;

    f->name = extract_string (line, 56, 0);
    dmsg ("name: [%s]\n", f->name);
    str_strip2 (f->name, "'");
    f->rights = 0644;

    return 0;
}

/*
IBM TCP/IP til OS/2 - FTP-klient (18:09:38 on Jun 18 1996)
Forbundet med 10.38.10.1.
220-TCPFTP IBM MVS V3R2 at DKTOPM01, 14:38:23 on 2000/01/31
220 Connection will close if idle for more than 5 minutes.
Navn (10.38.10.1): ksj
331 Send password please.
Kodeord: ........
230 KSJ is logged on.  Working directory is "KSJ.".
ftp> cd tup
250 "'KSJ.TUP.'" is working directory name prefix
ftp> cd jcl
250 "'KSJ.TUP.JCL'" partitioned data set is working directory
ftp> dir a*
200 Port request OK.
125 List started OK.
 Name     VV.MM    Created       Changed     Size  Init   Mod   Id
AAAAAA    01.01 1997/11/11 1998/04/01 13:14     5     3     0 KTN
ADRCOMPR  01.00 1991/05/13 1994/10/10 09:09    14    14     0 KXS
AJA       01.00 1997/11/11 1997/11/11 10:51     3     3     0 KTN
AJALIST   01.08 1994/04/07 1998/10/30 15:25     5    18     0 KSJ
AJL3800   01.00 1994/05/09 1994/05/09 11:19    24    24     0 KXS
ALIAS     01.14 1991/04/10 1995/09/21 10:44    23    32     0 KTN
ALIAS2    01.00 1996/03/19 1996/03/19 19:16    17    17     0 KTN
ALLOPDS   01.03 1991/05/04 1994/09/08 08:59    21    12    12 KXS
ALTER     01.09 1991/11/25 1995/05/29 13:39    49    10     0 KTN
AMBLIST   01.08 1993/08/16 1999/04/28 11:06     8     8     0 KSJ
ARCTEST   01.00 1996/06/06 1996/06/06 14:28    31    31     0 KTN
ASIDJCL   01.00 1995/09/27 1995/09/27 11:02     5     5     0 KTN
ASLIST    01.06 1993/03/11 1993/11/30 09:42    20    16     0 KXS
ASMJCL    01.05 1992/12/11 1998/05/15 14:54    15    17     0 KTN
ASTEXBI   01.01 1997/01/16 1997/01/16 09:34    46    46     0 KTN
ASXCNTL   01.04 1993/10/29 1999/04/22 16:09    20    20     5 KSJ
ASXKLAR   01.04 1996/12/16 1996/12/16 12:34    43    43     0 KTN
ASXSTAT1  01.03 1994/02/23 1994/03/14 13:04    88   190    68 KSJ
ASXTEST   01.00 1994/02/23 1994/02/23 13:32   190   190     0 KXS
ASXTEST2  01.01 1996/06/06 1996/06/06 13:54    70    65     0 KTN
ASXTREND  01.01 1996/02/12 1996/02/12 09:32    42    42     0 KTN
ASXTRENF  01.02 1996/02/12 1996/02/12 10:47    46    46     0 KTN
ASXTREN2  01.00 1997/05/16 1997/05/16 09:31    52    52     0 KTN
AVRS      01.00 1997/10/01 1997/10/01 15:12    84    84     0 KTN
250 List completed successfully.
*/

static int parse_mvs2 (char *line, file *f)
{
    dmsg ("MVS2 parser\n");
    if (strstr (line, "Name") && strstr (line, "VV.MM") && strstr (line, "Created")) return 1;

    if (strlen(line) < 61) return 1;

    f->flags = FILE_FLAG_REGULAR;

    f->size = extract_int64 (line, 43, 49) * LRECL;

    f->mtime = parse_date_time (DATE_FMT_8, line+27, line+38);

    line[9] = '\0';
    str_strip (line, " ");
    f->name = strdup (line);
    dmsg ("name: [%s]\n", f->name);
    f->rights = 0644;

    return 0;
}

/*
<!-- HTML listing generated by Inktomi Traffic Server -->
<!-- Mon, 07 Feb 2000 13:22:21 GMT -->
<HTML>
<HEAD>
<TITLE>
Ftp Directory
ftp://ftp.cdrom.com:21/%2f/
</TITLE>
</HEAD>
<BODY>
<H2>
Current Directory ftp://ftp.cdrom.com:21/%2f/
</H2>
<PRE>
total 35975
<A HREF="..">Parent Directory</A>
<A HREF="ftp://ftp.cdrom.com:21/%2f/README">README</A>                         696 bytes  Nov 19 1997  ]
<A HREF="ftp://ftp.cdrom.com:21/%2f/UPLOADS.TXT">UPLOADS.TXT</A>                      2 KB     Sep 26 1998   ]
<A HREF="ftp://ftp.cdrom.com:21/%2f/archive-info/">archive-info</A>                              Oct 05 1998   Directory]
<A HREF="ftp://ftp.cdrom.com:21/%2f/catalog.txt">catalog.txt</A>                               Nov 22 1998   Symbolic link]
<A HREF="ftp://ftp.cdrom.com:21/%2f/config.txt">config.txt</A>                                Nov 22 1998   Symbolic link]
<A HREF="ftp://ftp.cdrom.com:21/%2f/etc/">etc</A>                                       May 02 1999   Directory]
<A HREF="ftp://ftp.cdrom.com:21/%2f/ls-lR">ls-lR</A>                           19 MB     Feb 07 13:13  ]
<A HREF="ftp://ftp.cdrom.com:21/%2f/ls-lR.gz">ls-lR.gz</A>                        15 MB     Feb 06 14:24  ]
<A HREF="ftp://ftp.cdrom.com:21/%2f/pub/">pub</A>                                       Dec 20 23:43  Directory]
</PRE>]
</BODY>]
</HTML>]
*/

static int parse_inktomi (char *line, file *f)
{
    char     *p, *p1;

    // check for line starting with <A HREF=
    if (str_headcmp (line, "<A HREF=")) return 1;

    // find the start of the link body
    p = strchr (line, '>');
    if (p == NULL) return 1;
    p++;
    p1 = strstr (p, "</A>");
    if (p1 == NULL) return 1;
    *p1 = '\0';
    f->name  = strdup (p);

    // find the size (if it present)
    p = p1+4;
    while (*p && *p == ' ') p++; // skip whitespace
    if (*p == '\0') return 1;
    if (isdigit ((unsigned char)*p))
    {
        p1 = strchr (p, ' ');
        if (p1 == NULL) return 1;
        *p1++ = '\0';
        f->size = atol (p);
        if (str_headcmp (p1, "bytes")   == 0) f->size *= 1;
        else if (str_headcmp (p1, "KB") == 0) f->size *= 1024;
        else if (str_headcmp (p1, "MB") == 0) f->size *= 1024*1024;
        else if (str_headcmp (p1, "GB") == 0) f->size *= 1024*1024*1024;
        else return 1;
        p = strchr (p1, ' ');
        if (p == NULL) return 1;
        while (*p && *p == ' ') p++; // skip whitespace
        if (*p == '\0') return 1;
    }
    else
    {
        // no size...
        f->size = 0;
    }

    // p now points to date
    f->mtime = parse_date_time (DATE_FMT_4, p, NULL);

    // type
    if (strlen (p) < 11) return 1;
    p += 11;
    if (strstr (p, "Directory") != NULL) f->flags = FILE_FLAG_DIR;
    else if (strstr (p, "Symbolic link") != NULL) f->flags = FILE_FLAG_LINK;
    else f->flags = FILE_FLAG_REGULAR;

    // not really available
    f->rights = (f->flags & FILE_FLAG_DIR) ? 0755 : 0644;

    return 0;
}

/*
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
<HTML><HEAD><TITLE>ftp://ftp.sai.msu.su/</TITLE>
<BASE HREF="ftp://ftp.sai.msu.su/"></HEAD>
<BODY><H2>Directory of <A HREF="/">ftp://ftp.sai.msu.su</A>/</H2>
<HR><PRE>total 753
drwxr-xr-x   6 root     root         1024 Mar 26 22:20 <A HREF="./">.</A>
drwxr-xr-x   6 root     root         1024 Mar 26 22:20 <A HREF="../">..</A>
dr-xr-xr-x   2 root     root         1024 Dec 28  1996 <A HREF="bin/">bin</A>
dr-xr-xr-x   2 root     root         1024 Oct 24  1998 <A HREF="etc/">etc</A>
dr-xr-xr-x   2 root     root         1024 Oct 24  1998 <A HREF="lib/">lib</A>
-rw-r--r--   1 root     root       759851 Mar 26 22:19 <A HREF="ls-lR.gz">ls-lR.gz</A>
drwxrwxr-x  20 er       netadm       1024 Dec 25 08:40 <A HREF="pub/">pub</A>
</PRE><HR>
<ADDRESS>Apache/1.3.12 Server at rillanon.sai.msu.ru Port 80</ADDRESS>
</BODY></HTML>
*/

static int parse_apache (char *line, file *f)
{
    char     *p, *p1, *p2;
    int      rc;

    // kill <HR><PRE> if present
    if (str_headcmp (line, "<HR><PRE>") == 0) line += 9;
    // the listing is just like Unix, but with HREFs
    p = strdup (line);
    p1 = strstr (p, "<A HREF=");
    if (p1 == NULL) goto nothing;
    p2 = strchr (p1, '>');
    if (p1 == NULL) goto nothing;
    strcpy (p1, p2+1);
    p1 = strstr (p1, "</A>");
    if (p1 == NULL) goto nothing;
    *p1 = '\0';

    dmsg ("apache_parser: passing [%s] to unix parser\n", p);
    rc = parse_unix (p, f);
    free (p);
    return rc;

nothing:
    free (p);
    return 1;
}
