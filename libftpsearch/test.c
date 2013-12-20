#include <asvtools.h>
#include "fsearch.h"

int main (int argc, char *argv[])
{
    int rc;
    int req;

    tools_debug = fopen ("debug.txt", "w");
    req = FTPS_query ("live", 100, MT_MOVIE, ST_S_SUBSTR, 0, 0, NULL, NULL, NULL, NULL, 0, 0, 0, 0);
    //req = FTPS_query ("phantom.*(avi|mov|mpg)", 100, MT_FILE, ST_S_REGEXP, 0, 0, NULL, NULL, NULL, NULL, 0, 0, 0, 0);
    rc = FTPS_search ("ftpsearch.rambler.ru", 0, req, "search.txt", 32768, 1500.0);
    printf ("rc = %d\n", rc);
    FTPS_free (req);
    
    return 0;
}
