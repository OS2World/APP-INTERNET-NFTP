#include <asvtools.h>

int main (int argc, char *argv[])
{
    int i, rc;
    char *s;

    if (argc == 1) error1 ("usage: %s mimetype-file [MIME type]\n");

    rc = mime_load (argv[1]);
    if (rc != 0) error1 ("error loading mime types (rc=%d)\n", rc);
    printf ("%d types have been loaded\n", mime_ntypes());

    if (argc == 2) return 0;

    i = mime_name2num (argv[2]);
    printf ("id = %d for %s\n", i, argv[2]);
    if (i < 0) return -1;

    s = mime_num2name (i);
    printf ("type '%s' for %d\n", s, i);

    return 0;
}
