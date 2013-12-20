#include <asvtools.h>
#include <string.h>

int main (int argc, char *argv[])
{
    if (argc != 2) error1 ("one argument required");
    printf ("%s: %d\n", argv[1], is_unix_file_entry (argv[1]));
    return 0;
}
