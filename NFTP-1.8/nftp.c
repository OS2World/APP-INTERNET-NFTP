#include <fly/fly.h>

#include "nftp.h"
#include "auxprogs.h"

struct _display  display;
struct _options  options;
struct _cmdline  cmdline;
struct _sort     sort;
struct _paths    paths;

FILE             *dbfile;
int              keynames_running = 0;
struct _item     *main_menu = NULL;

/* -------------------------------------------------------------------- */

int fly_main (int argc, char *argv[], char **envp)
{
    int     key;

    initialize (&argc, &argv, &envp);
    
    key = init (argc, argv);
    if (key) do_key (key);
    update (1);
    
    while (1)
    {
        key = getmessage (-1);
        dmsg ("got message %d\n", key);
        do_key (key);
    }
}
