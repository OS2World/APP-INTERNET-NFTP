/*
 Usage:

 .    license -{N|O order-file|S license-file|B order-file|E license-id} [-MPR]
 .        [-F license-database] [-K private-key-file] [-T message-template]
 .        [-o registration-name] [-C customer-name] [-E customer-email]
 .        [-e expiration-date] [-n number-of-installations] [-t type]
 .        [-I ipaddr-list] [-H hostname-list]
 .        [-r reseller-id] [-m my-email]

 Modes of operation:

 N - create new license from command-line arguments (default)
 O - read ShareIt order from specified file and create new license
 I - read ShareIt order from specified order database file and create new licenses
 S - read license from specified file, assign license id and sign it
 B - read BMT Micro order from specified file and create new license
 X - extract license with specified id from database

 Destination (at least one of the options must be selected manually):

 M - mail license to given address
 P - print license to stdout

 R - don't record license in the database
 Y - cycle by records in the order database (BMT Micro, ShareIt2 mode)

 Options:

 F - specify file which holds information about issued licenses
 T - specify file with registration message template
 K - specify file with private key

 o - specify who is license owner (required); no default
 C - specify customer's name (required); default: same as license owner
 e - specify expiration date (YYYYMMDD); default: none
 n - specify number of installations; default: 1
 t - specify type; default: pl
 I - specify comma-separated list of IP addresses/masks; default: none
 H - specify comma-separated list of host names/masks; default: none
 E - specify customer's e-mail (required); no default
 r - specify reseller's id (required); default: 0

 m - specify my e-mail address; default: asv@ayukov.com

 ------------------------------------------------------------------------
 Created licenses are recorded in the file
 /home/asv/Texts/Misc/NFTP-keys/regcodes.txt
 This file also indicates what license ids are already used.
 This is comma-separated values file; its fields are (from first to last)

 - license id
 - license owner name (whom to license to)
 - customer's name
 - customer's e-mail
 - number of licenses
 - date of issue (YYYYMMDD)
 - license type
 - reseller id
 - IP address list (comma-separated)
 - hostname list (comma-separated)

 reseller id: 0 - direct sale, 1 - BMT Micro, 2 - ShareIt, 3 - ShaReg,
 4 - gift, 5 - postcard registration, 6 - reward (translator etc.)
 */

#include <asvtools.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <scl.h>

#include "licensing.h"

/* ----------------------------------------------------------------------- */

enum {MODE_NEWLICENSE, MODE_ORDER, MODE_SHAREIT, MODE_BMT, MODE_SIGN, MODE_EXTRACT};

static char *public_key =
    "BAA21E4345D2F414EB49D7692EF748EBA776F3FBDE6784E91D439083F"
    "CBD13AC53C3BEA089B06B731D8FFC0A94E94F8B0D903A1022200A9A78"
    "83906297F90AEAB6492CC9470370300441B30A0341977CFA97B537FE3"
    "3EA313C16155A2806BCA470DCC80E57ACB850AA6A9A6DBCF07C319661"
    "50FD1C27AD40DF09880FC8C518C5:010001";
static char *private_key = NULL;

static char *notsafe = ",";
static char *myaddr = "asv@ayukov.com";

static int *ids, n_ids, n_ids_a, ids_loaded=FALSE;

/* ----------------------------------------------------------------------- */

int read_ids (char *lic_file);
int get_id (void);
int read_key (char *keyfile);
int generate_license (int id, int numinst, int issue_date, int exp_date,
                      char *type, char *owner, char *comment, char *ip_list,
                      char *host_list, int reseller_id, char *customer,
                      char *email, char *lic_file, char *msg_file,
                      int mail_license, int print_license, int dont_record);
int read_shareit (FILE *fp, char **customer, char **owner,
                  char **email, int *numinst, char **comment);
int read_bmt (FILE *fp, char **customer, char **owner,
              char **email, int *numinst, char **comment);
int read_shareit_db (FILE *fp, char **customer, char **owner,
                     char **email, int *numinst, char **comment);
int read_license (char *lic_file, int *id, int *exp_date, char **type,
                  char **customer, char **owner, char **email, int *numinst,
                  char **comment, char **ip_list, char **host_list);

/* ----------------------------------------------------------------------- */

int main (int argc, char *argv[])
{
    int  c, rc, mode, mail_license, print_license, dont_record;
    char *progname, *ids_file, *key_file, *msg_file, *order_file, *lic_file;
    time_t t1;
    struct tm tm1;
    int  id, numinst, issue_date, exp_date, reseller_id, cycle;
    char *owner, *comment, *customer, *ip_list, *host_list, *email, *type;
    FILE *fp_orders;

    progname = argv[0];
    optind  = 1;
    opterr  = 0;

    time (&t1);
    tm1 = *gmtime (&t1);

    /* set up defaults */
    ids_file = "orders/regcodes.txt";
    key_file = "keys/secret";
    msg_file = "text/regmessage";
    order_file = NULL;
    lic_file = NULL;

    mode = MODE_NEWLICENSE;
    id = -1;
    numinst = 1;
    issue_date = (tm1.tm_year+1900)*100*100 + (tm1.tm_mon+1)*100 + tm1.tm_mday;
    exp_date = 0;
    type = "pl";
    owner = NULL;
    customer = NULL;
    comment = NULL;
    ip_list = NULL;
    host_list = NULL;
    email = NULL;
    reseller_id = 0;
    mail_license = FALSE;
    print_license = FALSE;
    dont_record = FALSE;
    cycle = FALSE;

    while ((c = getopt (argc, argv, "NSOL:B:X:o:e:n:t:I:H:E:r:m:F:K:T:MPC:R")) != -1)
    {
        switch (c)
        {
        case 'N': mode = MODE_NEWLICENSE; break;
        case 'S': mode = MODE_SHAREIT; order_file = strdup (optarg); cycle = TRUE; break;
        case 'O': mode = MODE_ORDER; order_file = strdup (optarg); break;
        case 'L': mode = MODE_SIGN; lic_file = strdup (optarg); break;
        case 'B': mode = MODE_BMT; order_file = strdup (optarg); cycle = TRUE; break;
        case 'X': mode = MODE_EXTRACT; break;

        case 'F': ids_file = strdup (optarg); break;
        case 'K': key_file = strdup (optarg); break;
        case 'T': msg_file = strdup (optarg); break;

        case 'C': customer = strdup (optarg); break;
        case 'o': owner = strdup (optarg); break;

        case 'e':
            exp_date = atoi (optarg);
            break;

        case 'n':
            numinst = atoi (optarg);
            break;

        case 't':
            type = strdup (optarg);
            break;

        case 'I': ip_list = strdup (optarg); break;
        case 'H': host_list = strdup (optarg); break;

        case 'E':
            email = strdup (optarg);
            break;

        case 'r':
            reseller_id = atoi (optarg);
            break;

        case 'm':
            myaddr = strdup (optarg);
            break;

        case 'M': mail_license = TRUE; break;
        case 'P': print_license = TRUE; break;
        case 'R': dont_record = TRUE; break;
        }
    }

    /* do some initializations */
    if (read_ids (ids_file) < 0) error1 ("failed to load list of assigned ids\n");
    if (read_key (key_file) < 0) error1 ("failed to load secret key\n");
    if (order_file != NULL)
    {
        fp_orders = fopen (order_file, "r");
        if (fp_orders == NULL)
        {
            error1 ("failed to open order file %s\n", order_file);
            return -1;
        }
    }
    else
        fp_orders = NULL;

    do
    {
        switch (mode)
        {
        case MODE_NEWLICENSE:
            /* we don't do anything special when creating license from
             command-line parameters */
            break;

        case MODE_ORDER:
            /* single ShareIt order */
            if (read_shareit (fp_orders, &customer, &owner, &email, &numinst, &comment) < 0)
                error1 ("failed to read information from ShareIt order\n");
            reseller_id = 2;
            break;

        case MODE_BMT:
            /* in BMT Micro order processing mode we read orders and
             assign license fields */
            rc = read_bmt (fp_orders, &customer, &owner, &email, &numinst, &comment);
            if (rc == -2) break;
            if (rc)
            {
                warning1 ("failed to read information from BMT Micro orders db\n");
                continue;
            }
            reseller_id = 1;
            break;

        case MODE_SHAREIT:
            /* multiple ShareIt orders in CSV file */
            rc = read_shareit_db (fp_orders, &customer, &owner, &email,
                                  &numinst, &comment);
            if (rc == -2) break;
            if (rc)
            {
                warning1 ("failed to read information from ShareIt orders db\n");
                continue;
            }
            reseller_id = 2;
            break;

        case MODE_SIGN:
            if (lic_file == NULL) error1 ("error: license file is not set\n");
            /* in signing mode we read information from supplied incomplete license
             file and do the rest */
            if (read_license (lic_file, &id, &exp_date, &type, &customer, &owner, &email,
                              &numinst, &comment, &ip_list, &host_list) < 0)
            {
                warning1 ("failed to read information from license file\n");
                return -1;
            }
            break;
        }

        if (generate_license (id, numinst, issue_date, exp_date, type,
                              owner, comment, ip_list, host_list,
                              reseller_id, customer, email, ids_file, msg_file,
                              mail_license, print_license, dont_record) == 0)
            warning1 ("-->> Success\n");
        else
            warning1 ("-->> Failed to sign and print license data\n");
    }
    while (cycle);

    warning1 ("all done!\n");
    return 0;
}

/* ----------------------------------------------------------------------- */

int read_ids (char *lic_file)
{
    FILE *fp;
    char buf[16384], *p;

    fp = fopen (lic_file, "r");
    if (fp == NULL)
    {
        warning1 ("cannot open license database %s\n", lic_file);
        return -1;
    }

    n_ids_a = 1024;
    n_ids = 0;
    ids = xmalloc (sizeof(int) * n_ids_a);

    while (fgets (buf, sizeof(buf), fp) != NULL)
    {
        p = strchr (buf, ',');
        if (p == NULL)
        {
            warning1 ("error parsing record in license database:\n%s\n", buf);
            return -2;
        }
        *p = '\0';
        if (n_ids == n_ids_a)
        {
            n_ids_a *= 2;
            ids = xrealloc (ids, sizeof(int) * n_ids_a);
        }
        ids[n_ids++] = atoi (buf);
    }

    fclose (fp);
    qsort (ids, n_ids, sizeof(int), cmp_integers);
    ids_loaded = TRUE;

    return 0;
}

/* ----------------------------------------------------------------------- */

int get_id (void)
{
    int id, *ip;

    if (!ids_loaded) error1 ("internal error; id list not loaded\n");

    /* get a random value */
    srandom (time (NULL));
    id = (random () % 999999) + 1;

    /* check that this value is not used */
    if (n_ids == 0) goto found_good;
    ip = bsearch (&id, ids, n_ids, sizeof(int), cmp_integers);
    if (ip == NULL) goto found_good;

    do
    {
        id++;
        if (id == 1000000) id = 1;
    }
    while (bsearch (&id, ids, n_ids, sizeof(int), cmp_integers) != NULL);

found_good:
    if (n_ids == n_ids_a)
    {
        n_ids_a *= 2;
        ids = xrealloc (ids, sizeof(int) * n_ids_a);
    }
    ids[n_ids++] = id;
    qsort (ids, n_ids, sizeof(int), cmp_integers);
    return id;
}

/* ----------------------------------------------------------------------- */

int read_key (char *keyfile)
{
    char buf[8192];
    FILE *fp;

    /* reading secret key from disk :-) */
    fp = fopen (keyfile, "r");
    if (fp == NULL)
    {
        warning1 ("cannot open secret key file %s\n", keyfile);
        return -1;
    }

    if (fgets (buf, sizeof(buf), fp) == NULL)
    {
        warning1 ("error reading secret key!\n");
        return -2;
    }
    fclose (fp);

    str_strip (buf, " \r\n");
    private_key = strdup (buf);
    return 0;
}

/* ----------------------------------------------------------------------- */
#define MAX_LIC_LENGTH 16384

#define copy_bytes(s,n) \
    { \
    if (j+n >= nb_a-1) \
    { \
    nb_a = max1 (nb_a*2, nb_a + n); \
    msg = xrealloc (msg, nb_a); \
    } \
    memcpy (msg+j, s, n); \
    j += n; \
    }

int generate_license (int id, int numinst, int issue_date, int exp_date,
                      char *type, char *owner, char *comment, char *ip_list,
                      char *host_list, int reseller_id, char *customer,
                      char *email, char *lic_file, char *msg_file,
                      int mail_license, int print_license, int dont_record)
{
    char buf[8192], *lines[48];
    char *signature, *p, *license, *msg_template, *msg, *separator;
    int  i, j, n, nb, nb_a;
    FILE *fp;

    /* all fields should be filled by now. we check that all required
     fields are present, print resulting license and write it to
     license journal */
    if (id == -1) id = get_id ();
    if (owner == NULL)
    {
        warning1 ("Registration name is not set\n");
        return -1;
    }
    if (customer == NULL) customer = strdup (owner);
    if (email == NULL)
    {
        warning1 ("Customer e-mail is not set\n");
        return -2;
    }

    n = 0;

    /* produce all lines which will comprise a license text */

    snprintf (buf, sizeof(buf), "v2");
    lines[n++] = strdup (buf);

    snprintf (buf, sizeof(buf), "N%u", id);
    lines[n++] = strdup (buf);

    if (numinst != 1 && numinst != 0)
    {
        snprintf (buf, sizeof(buf), "n%u", numinst);
        lines[n++] = strdup (buf);
    }

    snprintf (buf, sizeof(buf), "i%u", issue_date);
    lines[n++] = strdup (buf);

    if (exp_date != 0)
    {
        snprintf (buf, sizeof(buf), "e%u", exp_date);
        lines[n++] = strdup (buf);
    }

    snprintf (buf, sizeof(buf), "t%s", type);
    str_strip (buf, "\r\n ");
    lines[n++] = strdup (buf);

    snprintf (buf, sizeof(buf), "o%s", owner);
    str_strip (buf, "\r\n ");
    lines[n++] = strdup (buf);

    snprintf (buf, sizeof(buf), "C%s", customer);
    str_strip (buf, "\r\n ");
    lines[n++] = strdup (buf);

    snprintf (buf, sizeof(buf), "E%s", email);
    str_strip (buf, "\r\n ");
    lines[n++] = strdup (buf);

    if (comment != NULL)
    {
        snprintf (buf, sizeof(buf), "c%s", comment);
        str_strip (buf, "\r\n ");
        lines[n++] = strdup (buf);
    }

    if (ip_list != NULL)
    {
        snprintf (buf, sizeof(buf), "I%s", ip_list);
        str_strip (buf, "\r\n ");
        lines[n++] = strdup (buf);
    }

    if (host_list != NULL)
    {
        snprintf (buf, sizeof(buf), "H%s", host_list);
        str_strip (buf, "\r\n ");
        lines[n++] = strdup (buf);
    }

    /* compute signature */
    p = str_mjoin (lines, n, '\n');
    signature = rsa_sign (p, strlen (p), private_key);
    if (rsa_verify (p, strlen(p), signature, public_key) != 1)
    {
        warning1 ("License verification failed!\n");
        return -3;
    }
    free (p);

    /* print license text to temporary file and read it back into memory */
    fp = fopen ("nftp.license", "w+");
    if (fp == NULL)
    {
        warning1 ("cannot create 'nftp.license' in the current directory\n");
        return -4;
    }
    for (i=0; i<n; i++)
    {
        flongprint (fp, lines[i], 60);
        fprintf (fp, "\n");
    }
    fprintf (fp, "s");
    flongprint (fp, signature, 59);
    fprintf (fp, "\n");
    rewind (fp);
    license = xmalloc (MAX_LIC_LENGTH);
    nb = fread (license, 1, MAX_LIC_LENGTH, fp);
    license[nb] = '\0';
    fclose (fp);

    /* print license to stdout if specified */
    if (print_license)
    {
        printf ("%s", license);
    }

    /* mail new license (produce a file and mail it) */
    if (mail_license)
    {
        /* load message template */
        nb = load_file (msg_file, &msg_template);
        if (nb < 0)
        {
            warning1 ("failed to load message template from %s\n", msg_file);
            return -5;
        }

        /* substitute variables in message template */
        msg = xmalloc (nb*2);
        nb_a = nb;
        for (i=0, j=0; i<nb; i++)
        {
            if (msg_template[i] == '@' && msg_template[i+1] != '\0' && msg_template[i+2] == '@')
            {
                switch (msg_template[i+1])
                {
                case 'C':
                    copy_bytes (customer, strlen (customer));
                    i += 2;
                    break;

                case 'L':
                    copy_bytes (license, strlen (license));
                    i += 2;
                    break;

                default:
                    copy_bytes (msg_template+i, 1);
                }
            }
            else
            {
                copy_bytes (msg_template+i, 1);
            }
        }
        msg[j] = '\0';

        /* print information and ask for confirmation before mailing */
        printf ("------------- Registration Code ------------\n"
                "%s\n"
                "--------- End of the Registration Code -----\n"
                "Press Enter to send this to %s (%s), Ctrl-C to cancel\n",
                license, customer, email);
        fgets (buf, sizeof(buf), stdin);

        /* create registration message */
        separator = "MIME-Separator-2833784798753847543";
        fp = fopen ("message.txt", "w");
        if (fp == NULL)
        {
            warning1 ("failed to create message file 'message.txt'\n");
            return -6;
        }
        fprintf (fp,
                 "To: %s\n"
                 "Bcc: %s\n"
                 "From: %s\n"
                 "Subject: NFTP license\n"
                 "MIME-Version: 1.0\n"
                 "Content-Type: multipart/mixed; boundary=\"%s\"\n"
                 "\n"
                 "This message is in MIME format.\n"
                 "Use MIME-compatible reader to view it.\n"
                 "\n"
                 "--%s\n"
                 "Content-Type: text/plain; charset=US-ASCII\n"
                 "\n"
                 "%s\n"
                 "\n"
                 "--%s\n"
                 "Content-Type: text/plain; name=\"nftp.license\"\n"
                 "\n"
                 "%s\n"
                 "\n",
                 email, myaddr, myaddr, separator, separator,
                 msg, separator, license
                );
        fclose (fp);
        system ("sendmail -t <message.txt");
        //unlink ("message.txt");
    }

    /* register generated license in the database (only if this license
     was output in some way) */
    if (!dont_record)
    {
        fp = fopen (lic_file, "a");
        if (fp == NULL)
        {
            warning1 ("cannot open license database to append\n");
            return -7;
        }

        fprintf (fp, "%d,%s,%s,%s,%d,%d,%d,%s,%d,%s,%s,%s\n",
                 id,
                 owner == NULL ? "" : hexify2 (owner, notsafe),
                 customer == NULL ? "" : hexify2 (customer, notsafe),
                 email == NULL ? "" : hexify2 (email, notsafe),
                 numinst,
                 issue_date,
                 exp_date,
                 type == NULL ? "" : hexify2 (type, notsafe),
                 reseller_id,
                 ip_list == NULL ? "" : hexify2 (ip_list, notsafe),
                 host_list == NULL ? "" : hexify2 (host_list, notsafe),
                 comment == NULL ? "" : hexify2 (comment, notsafe)
                );

        fclose (fp);
    }

    return 0;
}

/* ----------------------------------------------------------------------- */

int read_shareit (FILE *fp, char **customer, char **owner,
                  char **email, int *numinst, char **comment)
{
    char buf[8192], *name, *value, *first=NULL, *last=NULL;
    char *street=NULL, *city=NULL, *country=NULL;
    int  product_id=0;

    while (fgets (buf, sizeof(buf), fp) != NULL)
    {
        str_strip (buf, "\r\n ");
        if (str_break_ini_line (buf, &name, &value) == 0)
        {
            if (strcmp (name, "Registration name") == 0)
                *owner = strdup (value);
            else if (strcmp (name, "E-Mail") == 0)
                *email = strdup (value);
            else if (strcmp (name, "First Name") == 0)
                first = strdup (value);
            else if (strcmp (name, "Last Name") == 0)
                last = strdup (value);
            else if (strcmp (name, "Number of licenses") == 0)
                *numinst = atoi (value);
            else if (strcmp (name, "Program") == 0)
                product_id = atoi (value);
            else if (strcmp (name, "Street") == 0)
                street = strdup (value);
            else if (strcmp (name, "FullCity") == 0)
                city = strdup (value);
            else if (strcmp (name, "Country") == 0)
                country = strdup (value);
            free (name);
            free (value);
        }
    }
    fclose (fp);

    if (product_id != 130599 && product_id != 131585 &&
        product_id != 131586 && product_id != 131587)
    {
        warning1 ("invalid product id=%d\n", product_id);
        return -1;
    }

    if (first != NULL && last != NULL)
    {
        *customer = str_join (first, " ");
        *customer = str_append (*customer, last);
    }
    if (first != NULL) free (first);
    if (last != NULL) free (last);

    if (street != NULL || city != NULL || country != NULL)
    {
        if (*comment != NULL) *comment = str_append (*comment, ", ");
        *comment = str_append (*comment, "Address: ");
        if (street != NULL)
        {
            *comment = str_append (*comment, street);
            *comment = str_append (*comment, " ");
        }
        if (city != NULL)
        {
            *comment = str_append (*comment, city);
            *comment = str_append (*comment, " ");
        }
        if (country != NULL)
        {
            *comment = str_append (*comment, country);
            *comment = str_append (*comment, " ");
        }
    }

    return 0;
}

/* ----------------------------------------------------------------------- */

int read_bmt (FILE *fp, char **customer, char **owner,
              char **email, int *numinst, char **comment)
{
    return 0;
}

/* ----------------------------------------------------------------------- */

int read_shareit_db (FILE *fp, char **customer, char **owner,
                     char **email, int *numinst, char **comment)
{
    return 0;
}

/* ----------------------------------------------------------------------- */

int read_license (char *lic_file, int *id, int *exp_date, char **type,
                  char **customer, char **owner, char **email,
                  int *numinst, char **comment,
                  char **ip_list, char **host_list)
{
    char *p, **lines;
    int  i, n, n1, nb;

    /* read entire input */
    nb = load_file (lic_file, &p);
    if (nb <= 0)
    {
        warning1 ("error reading from %s!\n", lic_file);
        return -1;
    }

    /* break license into lines */
    n = text2lines (p, &lines);
    if (n <= 0)
    {
        warning1 ("empty input\n");
        return -2;
    }

    /* refine input: drop empty lines, concatenate continued lines */
    n1 = 0;
    for (i=0; i<n; i++)
    {
        str_strip (lines[i], "\r\n ");
        /* skip empty lines */
        if (lines[i][0] == '\0' || lines[i][0] == 's') continue;
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

    /* analyze license records and assign variables */
    for (i=0; i<n; i++)
    {
        switch (lines[i][0])
        {
        case 'v':
            if (atoi (lines[i]+1) != 2)
                error1 ("license to sign has incompatible version %s", lines[i]+1);
            break;

        case 'N':
            *id = atoi (lines[i]+1);
            break;

        case 'n':
            *numinst = atoi (lines[i]+1);
            break;

        case 'i':
            /* ignore issue date */
            break;

        case 'e':
            *exp_date = atoi (lines[i]+1);
            break;

        case 't':
            *type = strdup (lines[i]+1);
            break;

        case 'o':
            *owner = strdup (lines[i]+1);
            break;

        case 'c':
            *comment = strdup (lines[i]+1);
            break;

        case 'C':
            *customer = strdup (lines[i]+1);
            break;

        case 'E':
            *email = strdup (lines[i]+1);
            break;

        case 's':
            /* ignore signature */
            break;

        case 'I':
            *ip_list = strdup (lines[i]+1);
            break;

        case 'H':
            *host_list = strdup (lines[i]+1);
            break;
        }
    }

    free (p);
    free (lines);
    return 0;
}

