#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <asvtools.h>
#include "manycode.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*
conv_table cp878[] =
{
    {0x80, 0x2500},
    {0x81, 0x2502},
    {0x82, 0x250C},
    {0x83, 0x2510},
    ...
    {0, 0}
};

*/

int main (int argc, char *argv[])
{
    int      i, n;
    int      *in;
    Wchar_t  *out;
    FILE     *fp;

    if (argc != 3) error1 ("incorrect parameters");
    n = ucm_parse (argv[1], &in, &out);
    if (n <= 0) error1 ("error loading %s\n", argv[1]);
    fp = fopen (argv[2], "w");
    if (fp == NULL) error1 ("failed to open %s", argv[2]);
    fprintf (fp, "{\n");
    for (i=0; i<n; i++)
    {
        fprintf (fp, "    {0x%02X, 0x%04X},\n", in[i], out[i]);
    }
    fprintf (fp, "    {0x%02X, 0x%04X},\n", 0, 0);
    fprintf (fp, "};\n");

    return 0;
}
