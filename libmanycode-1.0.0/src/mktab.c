#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int main (int argc, char *argv[])
{
    int i;
    
    if (argc < 2) return -1;
    
    for (i=1; i<argc; i++)
    {
        printf ("conv_table cp%s[] =\n", argv[i]);
        printf ("#include \"ic/ibm-%s.ic\"\n\n", argv[i]);
    }
    
    printf ("cp_table CVT0[] =\n");
    printf ("{\n");
    printf ("    {819, iso8859_1},\n");

    for (i=1; i<argc; i++)
    {
        printf ("    {%s, cp%s},\n", argv[i], argv[i]);
    }
    printf ("};\n");
    
    return 0;
}
