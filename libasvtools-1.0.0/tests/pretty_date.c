#include <asvtools.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    time_t t;

    t = atoi (argv[1]);
    printf ("%s\n", pretty_date(t));

    return 0;
}

