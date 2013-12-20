#include <asvtools.h>
#include <locale.h>

int main (int argc, char *argv[])
{
    regex_t rx;
    int     rc;
    regmatch_t pm;

    setlocale (LC_ALL, "ru_RU.KOI8-R");

    if (argc != 3) error1 ("usage: %s regexp sample\n", argv[0]);

    rc = regcomp (&rx, argv[1], REG_EXTENDED|REG_ICASE);
    if (rc != 0) error1 ("rc = %d from regcomp()\n", rc);

    rc = regexec (&rx, argv[2], 1, &pm, 0);
    if (rc != 0) error1 ("rc = %d from regexec()\n", rc);

    printf ("%d:%d\n", (int)pm.rm_so, (int)pm.rm_eo);

    return 0;
}
