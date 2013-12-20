#include <stdio.h>
#include <time.h>

void reg_convert (char *src, char *hash, char *dest);

int main (int argc, char *argv[])
{
    char           buffer[512], code[8];
    unsigned long  t = time (NULL);
    int            b1 = t % 26, b2 = (t/26) % 26, b3 = (t/26/26) % 26;
    
    code [0] = '@' + b1;
    code [1] = '@' + b2;
    code [2] = '@' + b3;
    code [3] = '\0';
    
    if (argc != 2) return 1;
    reg_convert (argv[1], code, buffer);
    printf ("name=%s\ncode=%s\n", argv[1], buffer);
    return 0;
}

/* -------------------------------------------------------------------
 converts src (user name) to dest (reg. code) using hash (only first
 three symbols of hash are used) */

void reg_convert (char *src, char *hash, char *dest)
{
    int code = hash[0] + hash[1] + hash[2];
    int i, nc, l = strlen (src), h, h0 = 0;
    
    nc = 16 + (hash[0] * hash[1] * hash[2]) % 8;
    
    dest[0] = hash[0];
    dest[1] = hash[1];
    dest[2] = hash[2];
    for (i=3; i<nc+3; i++)
    {
        h = src[i%l]*code + h0;
        h0 = h % 10;
        h = (h % (122-48+1)) + 48;
        if (h >= 58 && h <= 63) h -= 10;
        if (h >= 91 && h <= 96) h -= 6;
        dest[i] = (char) h;
    }
    dest[i] = '\0';
}
