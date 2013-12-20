#include <fly/fly.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <scl.h>

#include "nftp.h"
#include "auxprogs.h"
#include "net.h"

#define NP                 16
#define CURRENT_VERSION    1

#define ENC_DISABLED       0
#define ENC_NOTHING        1
#define ENC_NFTP           2
#define ENC_BLOWFISH       3

#define SYM_RANGE ('z'-'0')

typedef struct
{
    char *hostname;
    char *userid;
    char *password;
}
_hup;

static int     np=0, npa=0; // number of triplets, number of allocated cells
static _hup    *hup;
//static char   *trial = "";
static char   *keyphrase=NULL;
static int    changed = FALSE, maxwidth1, maxwidth2, psw_visible;

/* ------------------------------------------------------------------------ */

static void psw_create (void);
static void psw_free (void);
static void psw_write (void);
static void drw_psw (int n, int len, int shift, int pointer, int row, int col);
static int  cbk_psw (int *nchoices, int *n, int key);
static int  edit_entry (char **h, char **u, char **p);

/* ------------------------------------------------------------------------ */

static void psw_create (void)
{
    npa = NP;
    np = 0;
    hup = malloc (sizeof(_hup) * npa);
}
    
/* ------------------------------------------------------------------------ */

static void psw_free (void)
{
    int  i;
    
    // free previous values if necessary
    if (np != 0)
    {
        for (i=0; i<np; i++)
        {
            free (hup[i].hostname);
            free (hup[i].userid);
            free (hup[i].password);
        }
        np = 0;
    }
    if (npa != 0)
    {
        free (hup);
        npa = 0;
    }
}

/* ------------------------------------------------------------------------ */

static void add_entry (char *h, char *u, char *p)
{
    if (np >= npa-1)
    {
        npa *= 2;
        hup = realloc (hup, sizeof (_hup) * npa);
    }

    hup[np].hostname = strdup (h);
    hup[np].userid   = strdup (u);
    hup[np].password = strdup (p);
    np++;
}

/* ------------------------------------------------------------------------ */

void psw_read (void)
{
    FILE  *fp;
    int   version, enc_type;
    char  *h, *u, *p, *p1, *p2, *p3, *p4, *p5, buffer[1024];

    if (options.psw_enctype == 0) return;
    
    // create new data arrays
    psw_free ();
    psw_create ();

    // creating new passwords file
    if (access (options.psw_file, R_OK) != 0) return;
    
    // open passwords file
    fp = fopen (options.psw_file, "r");
    if (fp == NULL)
    {
        fly_ask_ok (ASK_WARN, M("Cannot open file '%s' for reading"), 
                    options.psw_file);
        return;
    }

    // read "version=1" string
    if (fgets (buffer, sizeof(buffer), fp) == NULL) goto broken_file;
    str_strip (buffer, " \n\r");
    if (buffer[0] == '\0' || str_headcmp (buffer, "version=") != 0) goto broken_file;
    version = atoi (buffer+8);
    if (version > CURRENT_VERSION)
    {
        fly_ask_ok (ASK_WARN, M("Version of the password file (%d) is not supported"), version);
        fclose (fp);
        return;
    }

    // read "encryption=1" string
    if (fgets (buffer, sizeof(buffer), fp) == NULL) goto broken_file;
    str_strip (buffer, " \n\r");
    if (buffer[0] == '\0' || str_headcmp (buffer, "encryption-type=") != 0) goto broken_file;
    enc_type = atoi (buffer+16);

    // read "keycheck=..." strings
    if (fgets (buffer, sizeof(buffer), fp) == NULL) goto broken_file;
    str_strip (buffer, " \n\r");
    if (buffer[0] == '\0' || str_headcmp (buffer, "keycheck1=") != 0) goto broken_file;
    p1 = strdup (buffer+10);
    if (fgets (buffer, sizeof(buffer), fp) == NULL) goto broken_file;
    str_strip (buffer, " \n\r");
    if (buffer[0] == '\0' || str_headcmp (buffer, "keycheck2=") != 0) goto broken_file;
    p2 = strdup (buffer+10);

enter_keyphrase:
    if (keyphrase == NULL && enc_type == ENC_BLOWFISH)
    {
        buffer[0] = '\0';
        if (entryfield (M("Enter keyphrase to decrypt your password file"),
                        buffer, sizeof(buffer), LI_INVISIBLE) == PRESSED_ESC)
        {
            fclose (fp);
            free (p1); free (p2);
            return;
        }
        keyphrase = strdup (buffer);
        p5 = sha1x (keyphrase);
        bf_setkey (p5);
    }
    if (enc_type == ENC_BLOWFISH)
    {
        p3 = bf_decrypt (p2);
        p4 = sha1x (p3);
        if (strcmp (p4, p1) != 0)
        {
            fly_ask_ok (ASK_WARN, M("Keyphrase is incorrect"), version);
            free (keyphrase); keyphrase = NULL;
            free (p3); free (p4);
            goto enter_keyphrase;
        }
    }
    free (p1); free (p2);
    
    while (1)
    {
        if (fgets (buffer, sizeof(buffer), fp) == NULL) break;
        str_strip (buffer, " \n\r");
        if (buffer[0] == '#' || buffer[0] == '\0') continue;
        if (str_headcmp (buffer, "site=") != 0) goto broken_file;
        h = buffer+5;
        u = strchr (h, ',');
        if (u == NULL) goto broken_file;
        *u++ = '\0';
        p = strchr (u, ',');
        if (p == NULL) goto broken_file;
        *p++ = '\0';
        hup[np].hostname = strdup (h);
        hup[np].userid = strdup (u);
        switch (enc_type)
        {
        case ENC_NFTP:
            hup[np].password = unscramble (p);
            break;
        case ENC_BLOWFISH:
            hup[np].password = bf_decrypt (p);
            break;
        case ENC_NOTHING:
        default:
            hup[np].password = strdup (p);
            break;
        }
        if (np == npa-1)
        {
            npa *= 2;
            hup = realloc (hup, sizeof (_hup) * npa);
        }
        np++;
    }
    
    if (enc_type != options.psw_enctype) psw_write ();
    return;

broken_file:
    fly_ask_ok (ASK_WARN, M("%s\n"
                            "Password file has invalid contents"), 
                options.psw_file);
    fclose (fp);
    psw_free ();
}

/* ------------------------------------------------------------------------ */

static void psw_write (void)
{
    FILE  *pf;
    char  *p, *p1, *p2, buffer[64];
    int   i;

    if (options.psw_enctype == 0) return;

    if (keyphrase == NULL && options.psw_enctype == ENC_BLOWFISH)
    {
        buffer[0] = '\0';
        if (entryfield (M("Enter keyphrase which will be used to encrypt your password file"),
                        buffer, sizeof(buffer), LI_INVISIBLE) == PRESSED_ESC) return;
        keyphrase = strdup (buffer);
        p1 = sha1x (keyphrase);
        bf_setkey (p1);
        free (p1);
    }
    
    pf = try_open (options.psw_file, "w");
    if (pf == NULL)
    {
        fly_ask_ok (ASK_WARN, M("Cannot open file '%s' for writing"), options.psw_file);
        return;
    }

    // write info
    fprintf (pf, "version=%d\n", CURRENT_VERSION);
    fprintf (pf, "encryption-type=%d\n", options.psw_enctype);
    if (options.psw_enctype == ENC_BLOWFISH)
    {
        p  = sha1x (keyphrase);
        p1 = sha1x (p);
        p2 = bf_encrypt (p);
        fprintf (pf, "keycheck1=%s\n", p1);
        fprintf (pf, "keycheck2=%s\n", p2);
        free (p); free (p1); free (p2);
    }
    else
    {
        fprintf (pf, "keycheck1=\n");
        fprintf (pf, "keycheck2=\n");
    }

    // write hup triads
    fprintf (pf, "\n");
    for (i=0; i<np; i++)
    {
        switch (options.psw_enctype)
        {
        case ENC_NFTP:      p = scramble (hup[i].password); break;
        case ENC_BLOWFISH:  p = bf_encrypt (hup[i].password); break;
        default:
        case ENC_NOTHING:   p = strdup (hup[i].password); break;
        }
        fprintf (pf, "site=%s,%s,%s\n", hup[i].hostname, hup[i].userid, p);
        free (p);
    }
    
    fclose (pf);
}

/* ------------------------------------------------------------------------ */

/* returns password for given host/userid pair; NULL if not found */
char *psw_match (char *h, char *u)
{
    int i;
    
    for (i=0; i<np; i++)
    {
        if (strcmp (h, hup[i].hostname) == 0 &&
            strcmp (u, hup[i].userid) == 0)
        {
            return hup[i].password;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------ */

/* returns -1 if cancelled by user, 0 otherwise */
int psw_add (char *h, char *u, char *p)
{
    int i;

    if (npa == 0) psw_create ();
    
    for (i=0; i<np; i++)
    {
        if (strcmp (h, hup[i].hostname) == 0 &&
            strcmp (u, hup[i].userid) == 0)
        {
            // password is already there?
            if (strcmp (p, hup[i].password) == 0) return 0;
            // ask user whether to replace old password
            free (hup[i].password);
            hup[i].password = strdup (p);
            return 0;
        }
    }

    // not found!
    add_entry (h, u, p);
    psw_write ();
    
    return 0;
}

/* ------------------------------------------------------------------------ */

void psw_edit (void)
{
    url_t u;
    int   a, i;

    if (options.psw_enctype == 0)
    {
        fly_ask_ok (0, M("Please enable password caching in nftp.ini"));
        return;
    }
    
    changed = FALSE;
    psw_visible = FALSE;
    
    maxwidth1 = 3;
    for (i=0; i<np; i++)
        maxwidth1 = max1 (maxwidth1, strlen (hup[i].userid));
    maxwidth1 = min1 (maxwidth1, 20);
    
    maxwidth2 = 3;
    for (i=0; i<np; i++)
        maxwidth2 = max1 (maxwidth2, strlen (hup[i].password));
    maxwidth2 = min1 (maxwidth2, 20);
    
    a = fly_choose (M("Passwords"), 0, &np, 0, drw_psw, cbk_psw);
    
    if (changed && fly_ask (ASK_YN, M("Save changes to password file?"), NULL)) psw_write ();
    else psw_read ();
    if (a >= 0)
    {
        init_url (&u);
        strcpy (u.hostname, hup[a].hostname);
        strcpy (u.userid, hup[a].userid);
        strcpy (u.password, hup[a].password);
        update (0);
        if (Edit_Site (&u, FALSE) != PRESSED_ENTER) return;
        Login (&u);
        if (options.login_bell) Bell (3);
    }
}

/* -------------------------------------------------------------------  */

static void drw_psw (int n, int len, int shift, int pointer, int row, int col)
{
    char buf[1024], slack1[100], slack2[100];
    int  l;

    memset (buf, ' ', len);
    l = maxwidth1 - strlen (hup[n].userid);
    l = min1 (max1 (0, l), sizeof(slack1)-1);
    memset (slack1, ' ', l);
    slack1[l] = '\0';
    l = maxwidth2 - strlen (hup[n].password);
    l = min1 (max1 (0, l), sizeof(slack2)-1);
    memset (slack2, ' ', l);
    slack2[l] = '\0';

    if (psw_visible)
        snprintf1 (buf, sizeof(buf), " %s%s %c %s%s %c %s ", hup[n].userid, slack1, fl_sym.v, hup[n].password, slack2, fl_sym.v, hup[n].hostname);
    else
        snprintf1 (buf, sizeof(buf), " %s%s %c %s ", hup[n].userid, slack1, fl_sym.v, hup[n].hostname);

    if (pointer)
        video_put_str_attr (buf, len, row, col, options.attr_bmrk_pointer);
    else
        video_put_str_attr (buf, len, row, col, options.attr_bmrk_back);
}

/* -------------------------------------------------------------------  */

static int  cbk_psw (int *nchoices, int *n, int key)
{
    int         rc, i;
    char        *h, *u, *p;
    
    if (IS_KEY (key))
    {
        switch (key)
        {
        case _F1:
            help (M_HLP_PASSWORDS);
            break;

        case _Delete:
            if (np == 0) break;
            if (!fly_ask (ASK_YN|ASK_WARN, 
                          M("%s @ %s\n"
                          "Delete password entry?"), NULL,
                          hup[*n].userid, hup[*n].hostname)) break;
            free (hup[*n].hostname);
            free (hup[*n].userid);
            free (hup[*n].password);
            for (i=*n; i<np-1; i++) hup[i] = hup[i+1];
            np--;
            changed = TRUE;
            break;

        case _Insert:
            h = strdup (""); u = strdup (""); p = strdup ("");
            rc = edit_entry (&h, &u, &p);
            if (rc == PRESSED_ENTER)
            {
                add_entry (h, u, p);
                changed = TRUE;
            }
            free (h); free (u); free (p);
            break;

        case _CtrlE:
            if (np == 0) break;
            rc = edit_entry (&(hup[*n].hostname), &(hup[*n].userid), &(hup[*n].password));
            if (rc == PRESSED_ENTER) changed = TRUE;
            break;

        case _CtrlP:
            psw_visible = !psw_visible;
        }
    }

    if (changed)
    {
        maxwidth1 = 3;
        for (i=0; i<np; i++)
            maxwidth1 = max1 (maxwidth1, strlen (hup[i].userid));
        maxwidth1 = min1 (maxwidth1, 20);

        maxwidth2 = 3;
        for (i=0; i<np; i++)
            maxwidth2 = max1 (maxwidth2, strlen (hup[i].password));
        maxwidth2 = min1 (maxwidth2, 20);
    }
    
    return -2;
}

/* -------------------------------------------------------------------  */

static int edit_entry (char **h, char **u, char **p)
{
    int        reply;
    entryfield_t sitebuf[3];

    sitebuf[0].banner = M ("Host name");
    sitebuf[0].type = EF_STRING;
    str_scopy (sitebuf[0].value.string, *h);
    sitebuf[0].flags = 0;
        
    sitebuf[1].banner = M ("Login (user name)");
    sitebuf[1].type = EF_STRING;
    str_scopy (sitebuf[1].value.string, *u);
    sitebuf[1].flags = 0;

    sitebuf[2].banner = M ("Password");
    sitebuf[2].type = EF_STRING;
    str_scopy (sitebuf[2].value.string, *p);
    sitebuf[2].flags = psw_visible ? 0 : LI_INVISIBLE;

    reply = mle2 (3, sitebuf, M("Use Tab/Shift-Tab or Up/Down arrows to move between fields"), 0);

    if (reply == PRESSED_ENTER)
    {
        free (*h); free (*u); free (*p);
        *h = strdup (sitebuf[0].value.string);
        *u = strdup (sitebuf[1].value.string);
        *p = strdup (sitebuf[2].value.string);
    }

    return reply;
}

/* -------------------------------------------------------------------  */

void psw_chkey (void)
{
    char  buffer[1024], *p;
    int   rc;

    if (options.psw_enctype != ENC_BLOWFISH)
    {
        fly_ask_ok (0, M("This is only used with method(s) %s (see nftp.ini)"), "3");
        return;
    }
    
    buffer[0] = '\0';
    if (keyphrase != NULL) strcpy (buffer, keyphrase);

    rc = entryfield (M("Enter keyphrase which will be used to encrypt your password file"), buffer, sizeof(buffer), LI_INVISIBLE);
    if (rc == PRESSED_ENTER && buffer[0] != '\0')
    {
        free (keyphrase);
        keyphrase = strdup (buffer);
        p = sha1x (keyphrase);
        bf_setkey (p);
        free (p);
        psw_write ();
    }
}
