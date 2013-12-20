#include <asvtools.h>

#include <sys/types.h>

int main (int argc, char *argv[])
{
    int  i;
    u_int64_t i64;
    u_int32_t i32;
    u_int16_t i16;
    u_int8_t  i8;
    char buf[1];

    for (i=0; i<256; i++)
    {
        printf ("%2x [", i);
        buf[0] = i;
        fprint_bits (stdout, buf, 1);
        printf ("]\n");
    }

    i64 = 1 + 2*256 + 3*65536 + 4 * 16777216 + 5*4294967296LL;
    printf ("64 bits: ");
    fprint_bits (stdout, (char *)&i64, 8);
    printf ("\n");

    i32 = 1 + 2*256 + 3*65536 + 4 * 16777216;
    printf ("32 bits: ");
    fprint_bits (stdout, (char *)&i32, 4);
    printf ("\n");

    i16 = 1 + 2*256;
    printf ("16 bits: ");
    fprint_bits (stdout, (char *)&i16, 2);
    printf ("\n");

    i8 = 1;
    printf (" 8 bits: ");
    fprint_bits (stdout, (char *)&i8, 1);
    printf ("\n");

    return 0;
}
