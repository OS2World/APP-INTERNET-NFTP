#include <asvtools.h>

int main (int argc, char *argv[])
{
    int     rc, len;
    char    *p, *enc;
    
    if (argc != 2) 
    {
        printf ("use: base64 <string to encode>");
        return -1;
    }

    enc = base64_encode(argv[1], -1);
    
    printf ("encoding   [%s]\nresults in [%s]\n", argv[1], enc);

    rc = base64_decode (enc, &p, &len);
    
    printf ("result: %d; length = %d; decoded = [%s]", rc, len, p);
    
    return 0;
}
