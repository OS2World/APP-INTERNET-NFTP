#include <asvtools.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    if (argc != 2 && argc != 3) error1 ("usage: %s number [64]\n", argv[0]);

    if (argc == 2)
    {
        int nb, nb1;
        u_int32_t n, n1;
        char buf[5];

        n = strtoul (argv[1], NULL, 10);
        printf ("integer to encode: %u\n", n);

        nb = vby_encode (buf, n);
        printf ("%d bytes used for encoding;\nresult=", nb);
        fprint_bits (stdout, buf, nb);
        printf ("\n");

        nb1 = vby_decode (buf, &n1);
        printf ("decoded integer: %u, %d bytes were scanned\n", n1, nb1);
        if (n == n1 && nb == nb1) printf ("Success\n");
    }
    else
    {
        int nb, nb1;
        u_int64_t n, n1;
        char buf[15];

        n = strtoq1 (argv[1], NULL, 10);
        printf ("integer to encode: %s\n", printf64(n));

        nb = vby_encode64 (buf, n);
        printf ("%d bytes used for encoding;\nresult=", nb);
        fprint_bits (stdout, buf, nb);
        printf ("\n");

        nb1 = vby_decode64 (buf, &n1);
        printf ("decoded integer: %s, %d bytes were scanned\n",
                printf64(n1), nb1);
        if (n == n1 && nb == nb1) printf ("Success\n");
    }

    return 0;

}
