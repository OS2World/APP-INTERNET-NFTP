#include <asvtools.h>

#include <stdlib.h>
#include <ctype.h>

int main (int argc, char *argv[])
{
    int           i, j, nprint, high, alnum, space;
    unsigned char *s;
    long          length;
    float         d;

    for (j=1; j<argc; j++)
    {
        length = load_file (argv[j], (char **)&s);
        if (length <= 0)
        {
            printf ("%s: file is empty of unreadable\n", argv[j]);
            continue;
        }
        printf ("%s: %lu bytes, ", argv[j], length);

        nprint = 0;
        high = 0;
        alnum = 0;
        space = 0;

        for (i=0; i<length; i++)
        {
            if (isprint (s[i])) nprint++;
            if (s[i] > 127) high++;
            if (isalnum (s[i])) alnum++;
            if (isspace (s[i])) space++;
        }

        d = binchars (s, length);
       /* if (d == 0.0) */
       /* { */
       /*    printf ("plaintext\n"); */
       /*    continue; */
       /* } */

        printf ("%.3f%% print, ", 100.*(float)nprint/(float)length);
        printf ("%.3f%% high, ",  100.*(float)high/(float)length);
        printf ("%.3f%% alnum, ", 100.*(float)alnum/(float)length);
        printf ("%.3f%% space, ", 100.*(float)space/(float)length);
        printf ("%.5g%% binary, ", d*100.);
        if (d*100.0 < 0.1) printf ("plaintext");
        printf ("\n");
    }

    return 0;
}

