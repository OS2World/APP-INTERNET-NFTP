#include <asvtools.h>

int main (int argc, char *argv[])
{
    if (argc != 2) 
    {
        printf ("need a name of tree to create");
        return -1;
    }
    printf ("make_subtree(\"%s\") returned %d\n", argv[1], make_subtree (argv[1]));
    return 0;
}
