#include <asvtools.h>

#include <stdlib.h>

int main (int argc, char *argv[])
{
    int          i, n_headers, rc;
    char         *s, *body;
    header_line_t *headers;
    long         l;

    l = load_file (argv[1], &s);
    printf ("length: %lu\n", l);

    rc = http_parse (s, l, &headers, &n_headers, &body);
    printf ("rc = %d\n", rc);
    if (rc < 0) return -1;

    printf ("Body length: %ld\n", l-(body-s));
    printf ("Headers:\n");
    for (i=0; i<n_headers; i++)
    {
        printf ("%d. Name = [%s], Value = [%s]\n", i+1,
                headers[i].name, headers[i].value);
    }

    printf ("\nBody:\n%s\n", body);

    return 0;
}

