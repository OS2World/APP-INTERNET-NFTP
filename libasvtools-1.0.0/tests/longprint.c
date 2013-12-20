#include <asvtools.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    if (argc != 3) error1 ("usage: longprint length string\n");
    flongprint (stdout, argv[2], atoi(argv[1]));
    return 0;
}
