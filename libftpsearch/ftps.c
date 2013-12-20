/*
 * Operation (actions: n s u):
 * ./ftps.exe action argument
 * use "" when argument consists of several strings
 *
 * Options:
 * -t searchtype (e, s, w, d, t)
 * -T depth (tree depth for treee request)
 * -m maxhits
 * -c (case sensitive when specified; default: no)
 * -U (exclude Unix distributions?)
 * -C country_filter (set of space-separated values)
 * -P path_filter (set of space-separated values)
 * -D domain_filter (set of space-separated values)
 * -a start_date (seconds of Unix epoch)
 * -A end_date (seconds of Unix epoch)
 * -s min_size (bytes)
 * -S max_size (bytes)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asvtools.h>

#include "fsearch.h"

typedef struct
{
    int distrib_exclude; /* 0 - no, 1 - yes, -1 - undefined */
    int include_archives; /* 0 - no, 1 - yes, -1 - undefined */
    char *path_filter; /* space-separated values; NULL if undefined */
    char *domain_filter; /* space-separated values; NULL if undefined */
    char *country_filter; /* space-separated values; NULL if undefined */
    long start_date, end_date; /* seconds of Unix epoch; -1 if undefined */
    long min_size, max_size; /* size limits in bytes; -1 if undefined */
}
filter;

struct
{
    int  req_type;
    char req_letter;
    int  numarg;
    char *title;
}
requests[] =
{
    {ST_NAVIGATE, 'n', 2, "Navigation"},
    {ST_EXACT,    'e', 1, "Exact search"},
    {ST_S_SUBSTR, 's', 1, "Substring search"},
    {ST_S_REGEXP, 'x', 1, "Regexp search"},
    {ST_S_GLOB,   'g', 1, "Globbing search"},
    {ST_M_SUBSTR, 'S', 1, "Substring match"},
    {ST_M_REGEXP, 'X', 1, "Regexp match"},
    {ST_M_GLOB,   'G', 1, "Globbing match"},
};
#define nrequests (sizeof(requests)/sizeof(requests[0]))

char *results_file = "results.tmp";

int main (int argc, char *argv[])
{
    int         i, j, r, rc, has_filter, numarg, n;
    filter      flt;
    int         max_hits, unix_exclude, tree_depth, case_sen, ftps_port;
    int         query, mmtype;
    char        c, request, search_type;
    char        *ftps_server;
    char        *target;
    ftps_search_results *R;

    tools_debug = fopen ("debug.txt", "w");
    
    /* our defaults */
    ftps_server = "ftpsearch.rambler.ru";
    ftps_port = 1525;
    target = NULL;
    search_type = '?';
    max_hits = 8192;
    tree_depth = 3;
    case_sen = 0;
    unix_exclude = 0;
    mmtype = MT_FILE;
    flt.distrib_exclude = -1;
    flt.include_archives = -1;
    flt.country_filter = NULL;
    flt.path_filter = NULL;
    flt.domain_filter = NULL;
    flt.start_date = -1;
    flt.end_date = -1;
    flt.min_size = -1;
    flt.max_size = -1;
    has_filter = FALSE;

    /* parse options */
    optind  = 1;
    opterr  = 0;
    while ((c = getopt (argc, argv, "F:p:t:T:m:P:D:a:A:s:S:M:bcU")) != -1)
    {
        switch (c)
        {
        case 'F':
            ftps_server = strdup (optarg);
            break;

        case 'p':
            ftps_port = atoi (optarg);
            break;

        case 't':
            search_type = optarg[0];
            break;

        case 'T':
            tree_depth = atoi (optarg);
            break;

        case 'm':
            max_hits = atoi (optarg);
            break;

        case 'c':
            case_sen = 1;
            break;

        case 'U':
            unix_exclude = 1;
            break;

        case 'C':
            flt.country_filter = strdup (optarg);
            has_filter = TRUE;
            break;
            
        case 'P':
            flt.path_filter = strdup (optarg);
            has_filter = TRUE;
            break;
            
        case 'D':
            flt.domain_filter = strdup (optarg);
            has_filter = TRUE;
            break;

        case 'a':
            flt.start_date = atol (optarg);
            has_filter = TRUE;
            break;
            
        case 'A':
            flt.end_date = atol (optarg);
            has_filter = TRUE;
            break;
            
        case 's':
            flt.min_size = atol (optarg);
            has_filter = TRUE;
            break;
            
        case 'S':
            flt.max_size = atol (optarg);
            has_filter = TRUE;
            break;

        case 'M':
            mmtype = atoi (optarg);
            break;
            
        default:
        case ':':
        case '?':
            error1 ("bad option\n");
        }
    }

    if (argv[optind] == NULL)
    {
        warning1 ("\n"
                  "Usage:\n"
                  "ftps.exe [options] request argument(s)\n"
                  "\n"
                  "Request:\n"
                 );
        
        for (i=0; i<nrequests; i++)
        {
            warning1 ("%c: %s\n", requests[i].req_letter, requests[i].title);
        }
        
        warning1 ("\n"
                  "Options:\n"
                  "-m maxhits\n"
                  "-c (case sensitive when specified; default: no)\n"
                  "-U (exclude Unix distributions?)\n"
                  "-C country_filter (set of space-separated values)\n"
                  "-P path_filter (set of space-separated values)\n"
                  "-D domain_filter (set of space-separated values)\n"
                  "-a start_date (seconds of Unix epoch)\n"
                  "-A end_date (seconds of Unix epoch)\n"
                  "-s min_size (bytes)\n"
                  "-S max_size (bytes)\n"
                  "-M multimedia-type (0-file(default), 1-pictures, 2-sound, 3-movie)\n"
                  "\n"
                  "Server options:\n"
                  "-F hostname (FTPS server to connect to; default: ftpsearch.rambler.ru)\n"
                  "-p port (port to connect to; default: 1525)\n"
                 );
        exit (1);
    }
    request = argv[optind][0];
    optind++;

    // see what request is specified
    for (r=0; r<nrequests; r++)
    {
        if (request == requests[r].req_letter) break;
    }
    if (r == nrequests)
    {
        warning1 ("wrong request type '%c'; allowed are:", request);
        for (i=0; i<sizeof(requests)/sizeof(requests[0]); i++)
            warning1 (" %c", requests[i].req_letter);
        error1 ("\n");
    }

    // check number of request argument(s)
    numarg = 0;
    j = optind;
    while (argv[j++]) numarg++;
    warning1 ("found %d arguments\n", numarg);
    if (numarg != requests[r].numarg)
        error1 ("wrong number of request arguments for '%s'\n", requests[r].title);

    switch (requests[r].numarg)
    {
    case 1:  target = strdup (argv[optind]); break;
    case 2:  target = str_join (argv[optind], argv[optind+1]); break;
    default: error1 ("wrong number of arguments for this call\n");
    }
    
    warning1 ("Sending request '%s' with the following parameters:\n"
              "  target = %s\n"
              "  search_type = %c; tree_depth = %d; max_hits = %d; case_sen = %d\n",
              requests[r].title, target, search_type,
              tree_depth, max_hits, case_sen);
    
    if (has_filter)
    {
        warning1 ("Filter parameters: distrib_exclude=%d\n"
                  "  path_filter=(%s), domain_filter=(%s), country_filter=(%s)\n"
                  "  start_date=%ld, end_date=%ld; min_size=%ld, max_size=%ld\n",
                  flt.distrib_exclude,
                  flt.path_filter, flt.domain_filter, flt.country_filter,
                  flt.start_date, flt.end_date, flt.min_size, flt.max_size
                  );
    }

    query = FTPS_query (target, max_hits, mmtype, requests[r].req_type, case_sen,
                        unix_exclude, NULL, NULL, NULL, NULL,
                        0, 0, 0, 0);

    rc = FTPS_search (ftps_server, ftps_port, query, results_file, max_hits, 15.0);

    warning1 ("FTPS_search() returned %d\n", rc);
    if (rc) return rc;

    n = FTPS_num_results (query);
    warning1 ("%d results\n", n);
    if (n <= 0) return -1;

    R = FTPS_get_handle (query);
    for (i=0; i<n; i++)
    {
        if (R[i].userid != NULL || R[i].password != NULL)
            printf ("%s:%s @ ", R[i].userid, R[i].password);
        printf ("%s:%d  %s/%s", R[i].hostname, R[i].port, R[i].pathname, R[i].filename);
        if (S_ISREG(R[i].rights))
            printf (" %ld bytes", R[i].size);
        printf (" t=%lu", R[i].timestamp);
        printf ("\n");
    }
    
    return 0;
}
