#include <fly/fly.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <scl.h>

#include "nftp.h"
#include "network.h"
#include "licensing.h"

void new_check_licensing (char *user_path, char *sys_path);

license_t License;

/* -------------------------------------------------------------------
 converts src (user name) to dest (reg. code) using hash (only first
 three symbols of hash are used) */
void reg_convert (char *src, char *hash, char *dest)
{
    int code = hash[0] + hash[1] + hash[2];
    int i, nc, l = strlen (src), h, h0 = 0;

    nc = 16 + (hash[0] * hash[1] * hash[2]) % 8;

    dest[0] = hash[0];
    dest[1] = hash[1];
    dest[2] = hash[2];
    for (i=3; i<nc+3; i++)
    {
        h = src[i%l]*code + h0;
        h0 = h % 10;
        h = (h % (122-48+1)) + 48;
        if (h >= 58 && h <= 63) h -= 10;
        if (h >= 91 && h <= 96) h -= 6;
        dest[i] = (char) h;
    }
    dest[i] = '\0';
}

/* ------------------------------------------------------------------- */

void check_licensing (char *user_path, char *sys_path)
{
    char        converted[512];
    char        *name, *code;

    new_check_licensing (user_path, sys_path);
    if (License.status == LIC_VERIFIED || License.status == LIC_DELAYED) return;
    License.version = 1;
    
    name = cfg_get_string (CONFIG_NFTP, NULL, "registration-name");
    code = cfg_get_string (CONFIG_NFTP, NULL, "registration-code");

    if (*name == '\0' || *code == '\0') return;
    if (strlen (code) < 4) return;
    reg_convert (name, code, converted);
    if (strcmp (code, converted) == 0)
    {
        License.status = LIC_VERIFIED;
        License.name = strdup (name);
        return;
    }
}

/* ------------------------------------------------------------------- */
// try to find the license file:
// All systems  : 1) $LICENSES/nftp.license
//                2) $USER_LIBPATH/nftp.license
//
// Unix:          3) $SYSTEM_LIBPATH/nftp.license
//                4) /etc/nftp.license
//                5) /etc/licenses/nftp.license

static int revoked[] = {0};

static char *public_key =
    "BAA21E4345D2F414EB49D7692EF748EBA776F3FBDE6784E91D439083F"
    "CBD13AC53C3BEA089B06B731D8FFC0A94E94F8B0D903A1022200A9A78"
    "83906297F90AEAB6492CC9470370300441B30A0341977CFA97B537FE3"
    "3EA313C16155A2806BCA470DCC80E57ACB850AA6A9A6DBCF07C319661"
    "50FD1C27AD40DF09880FC8C518C5:010001";

void new_check_licensing (char *user_path, char *sys_path)
{
    char        license_file[8192], *p;
    char        **lines;
    int         i, n, n1, TTL, now;
    time_t      t1;
    struct tm   tm1;

    // set all fields to default values
    License.status   = LIC_NO;
    License.version  = 0;
    License.no       = -1;
    License.quantity = 1;
    License.type     = NULL;
    License.issue    = 0;
    License.expiry   = 0;
    License.n_ip_masks   = 0;
    License.n_host_masks = 0;
    License.ip_masks    = NULL;
    License.host_masks  = NULL;
    License.name = NULL;
    License.sign = NULL;
    
    // check LICENSES variable first if present
    p = getenv ("LICENSES");
    if (p != NULL)
    {
        strcpy (license_file, p);
        str_cats (license_file, "nftp.license");
        if (access (license_file, R_OK) == 0) goto found;
    }

    strcpy (license_file, user_path);
    str_cats (license_file, "nftp.license");
    if (access (license_file, R_OK) == 0) goto found;

    if (!fl_opt.has_driveletters)
    {
        strcpy (license_file, sys_path);
        str_cats (license_file, "nftp.license");
        if (access (license_file, R_OK) == 0) goto found;

        strcpy (license_file, "/etc/nftp.license");
        if (access (license_file, R_OK) == 0) goto found;

        strcpy (license_file, "/etc/licenses/nftp.license");
        if (access (license_file, R_OK) == 0) goto found;
    }

    return; // new license file not found

found:
    if (fl_opt.has_console && !fl_opt.initialized)
        fly_ask_ok (0, "loading %s......\n", license_file);

    n = load_textfile (license_file, &lines);
    n1 = 0;
    for (i=0; i<n; i++)
    {
        str_strip (lines[i], "\r\n ");
        /* skip empty lines */
        if (lines[i][0] == '\0') continue;
        /* skip lines with delimiters */
        if (str_headcmp (lines[i], "------") == 0) continue;
        /* see if previous line had a continuation mark */
        if (n1 > 0 && str_tailcmp (lines[n1-1], "\\") == 0)
        {
            /* append current line to previous */
            str_strip (lines[n1-1], "\\");
            lines[n1-1] = str_join (lines[n1-1], lines[i]);
        }
        else
        {
            lines[n1++] = lines[i];
        }
    }
    n = n1;
    if (n <= 0) return;

    //printf ("license:\n");
    //for (i=0; i<n; i++)
    //{
    //    printf ("%d. '%s'\n", i, lines[i]);
    //}

    for (i=0; i<n; i++)
    {
        switch (lines[i][0])
        {
        case 'v': License.version  = atoi (lines[i]+1); break;
        case 'N': License.no       = atoi (lines[i]+1); break;
        case 'n': License.quantity = atoi (lines[i]+1); break;
        case 'i': License.issue    = atoi (lines[i]+1); break;
        case 'e': License.expiry   = atoi (lines[i]+1); break;
        case 't': License.type     = strdup (lines[i]+1); break;
        case 'o': License.name     = strdup (lines[i]+1); break;
        case 's': License.sign     = strdup (lines[i]+1); break;
        case 'I': License.n_ip_masks   = str_parseline (strdup (lines[i]+1), &License.ip_masks, ',');
        case 'H': License.n_host_masks = str_parseline (strdup (lines[i]+1), &License.host_masks, ',');
        }
    }

    /* see if required fields are present */
    if (License.no <= 0 ||
        License.issue == 0 ||
        License.sign == NULL || License.sign[0] == '\0' ||
        License.name == NULL || License.name[0] == '\0' ||
        License.type == NULL || License.type[0] == '\0')
    {
        warning1 ("This license file is invalid\n");
        return;
    }

    // check license integrity. we use 'n-1' because signature itself
    // is excluded from the part being verified!
    p = str_mjoin (lines, n-1, '\n');
    if (rsa_verify (p, strlen (p), License.sign, public_key) != 1)
    {
        warning1 ("This license file is corrupted!\nLicense contents:\n%s\n", p);
        License.status = LIC_NO;
        return;
    }
    free (p);

    // check license expiration. time_now: 19991210
    if (License.expiry != 0)
    {
        time (&t1);
        tm1 = *gmtime (&t1);
        now = (tm1.tm_year+1900)*100*100 + (tm1.tm_mon+1)*100 + tm1.tm_mday;
        // TTL is time-to-live, in days
        TTL = License.expiry - now;
        if (TTL < 0)
        {
            warning1 ("This license has expired\n");
            License.status = LIC_NO;
            return;
        }
        if (TTL >= 0 && TTL < 10)
            warning1 ("This license is about to expire in %d day(s)!\n", TTL);
    }

    /* check revoked license numbers */
    for (i=0; i<sizeof(revoked)/sizeof(revoked[0]); i++)
    {
        if (revoked[i] == License.no)
        {
            warning1 ("This license has been revoked\n");
            License.status = LIC_NO;
            return;
        }
    }

    if (License.n_ip_masks != 0 || License.n_host_masks != 0)
        License.status = LIC_DELAYED;
    else
        License.status = LIC_VERIFIED;

    //warning1 ("status = %d, n_ip=%d, n_hosts=%d\n", License.status,
    //      License.n_ip_masks, License.n_host_masks);
}
