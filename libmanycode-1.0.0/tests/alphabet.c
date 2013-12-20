#include <stdio.h>

#define KOI8_STRING_FUNCTIONS
#include <manycode.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int main (int argc, char *argv[])
{
    int i;

    for (i=0; i<256; i++)
    {
        if (i != 0 && i % 8 == 0) printf ("\n");

        printf ("%02x=", i);
        if (i == '\'')      printf ("'\\'', ");
        else if (i == '\\') printf ("'\\\\', ");
        else if (i == '\t') printf ("'\\t', ");
        else if (i == '\n') printf ("'\\n', ");
        else if (i == '\b') printf ("'\\b', ");
        else if (i == '\r') printf ("'\\r', ");
        else if (i == '\f') printf ("'\\f', ");
        else if (i == '\v') printf ("'\\v', ");
        else if (i == ' ')  printf (" ' ', ");
        else if (i == 127)  printf ("0x%02x, ", i);
        else if (isprint (i))
            printf (" '%c', ", i);
        else
            printf ("0x%02x, ", i);
    }
}
