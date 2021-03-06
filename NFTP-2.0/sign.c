#include <asvtools.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <scl.h>

#include "licensing.h"

static char *public_key =
    "BAA21E4345D2F414EB49D7692EF748EBA776F3FBDE6784E91D439083F"
    "CBD13AC53C3BEA089B06B731D8FFC0A94E94F8B0D903A1022200A9A78"
    "83906297F90AEAB6492CC9470370300441B30A0341977CFA97B537FE3"
    "3EA313C16155A2806BCA470DCC80E57ACB850AA6A9A6DBCF07C319661"
    "50FD1C27AD40DF09880FC8C518C5:010001";

static char *license_file = "license.nftp";

int main (int argc, char *argv[])
{
    char      **lines, buffer[8192], *p, *private_key, *signature;
    int       i, n, n1;
    FILE      *fp;

    if (argc != 2) error1 ("one argument is needed");

    // reading secret key from disk :-)
    fp = fopen ("E:\\texts\\NFTP-keys\\secret", "r");
    if (fp == NULL) error1 ("cannot open key file!\n");
    if (fgets (buffer, sizeof(buffer), fp) == NULL) error1 ("error reading key file\n");
    fclose (fp);
    str_strip (buffer, " \r\n");
    private_key = strdup (buffer);
    printf ("key = %s\n", private_key);

    // reading license prototype file to sign
    n = load_textfile (argv[1], &lines);
    if (n <= 0) error1 ("cannot open %s\n", argv[1]);
    for (i=0; i<n; i++) str_strip (lines[i], "\r\n ");
    // delete signature if present
    n1 = 0;
    for (i=0; i<n; i++)
        if (lines[i][0] != '\0' && lines[i][0] != 's') lines[n1++] = lines[i];
    n = n1;

    // compute signature
    p = str_mjoin (lines, n, '\n');
    warning1 ("signing [%s], %d bytes\n", p, strlen(p));
    signature = rsa_sign (p, strlen (p), private_key);
    //free (p);

    // write new license
    fp = fopen (license_file, "w");
    if (fp == NULL) error1 ("cannot open %s", license_file);
    for (i=0; i<n; i++)
        fprintf (fp, "%s\n", lines[i]);
    fprintf (fp, "s%s\n", signature);
    fclose (fp);

    warning1 ("signature: %s\n", signature);

    warning1 ("verification result: %d\n",
              rsa_verify (p, strlen(p), signature, public_key));

    return 0;
}
