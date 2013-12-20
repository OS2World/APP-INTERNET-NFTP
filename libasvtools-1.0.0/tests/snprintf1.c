#include <asvtools.h>
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int main (int argc, char *argv[])
{
    char buf[32], *str;
    int  n;

    /* 1. output is larger than buffer */
    n = snprintf1 (buf, 16, "%s", "012345678901234567890123456789");
    printf ("%s - rc=%d\n", buf, n);

    /* 2. output is larger than 1MB. must fail on systems which do not
     have snprintf */
    str = xmalloc (1024*1024+2);
    memset (str, 'x', 1024*1024+2);
    str[1024*1024+1] = '\0';
    n = snprintf1 (buf, 16, "%s", str);
    printf ("%s - rc=%d\n", buf, n);

    return 0;
}
