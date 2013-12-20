#include <asvtools.h>

#include <string.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    int    port, s;
    char   *host;
    double timeout, t1;

    if (argc != 4) error1 ("usage: connect hostname port timeout\n");

    host = strdup (argv[1]);
    port = atoi (argv[2]);
    if (port <= 0) error1 ("port must be positive\n");
    timeout = atof (argv[3]);

    printf ("connecting to %s:%d, timeout %.3f sec...\n", host, port, timeout);
    Set_TCPIP_timeout (timeout);
    t1 = clock1 ();
    s = Connect (host, port);
    printf ("%sconnected (%d); %.3f sec\n",
            s < 0 ? "NOT " : "", s, clock1()-t1);
    Close (s);

    return 0;
}
