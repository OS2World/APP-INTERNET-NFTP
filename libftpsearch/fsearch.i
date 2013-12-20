%module fsearch
%{
#include "fsearch.h"
#include "fsearch_perl.h"
%}
%include typemaps.i

int FTPS_query (char *target, int maxhits, int mmtype,
                int req_type, int case_sen, int distrib_exclude,
                char *path_inc, char *path_exc,
                char *dom_inc, char *dom_exc,
                long start_date, long end_date,
                long min_size, long max_size);

int FTPS_search (char *search_server, int search_port, int shandle, char *filename, int maxhits, double timeout);
int FTPS_load (char *filename);
int FTPS_sort (int shandle, int parm1, int parm2, int parm3, int parm4, int parm5);
int FTPS_free (int shandle);
int FTPS_num_results (int shandle);
int FTPS_delete (int shandle);

char   *FTPS_get_hostname (int shandle, int n);
char   *FTPS_get_pathname (int shandle, int n);
char   *FTPS_get_filename (int shandle, int n);
char   *FTPS_get_login (int shandle, int n);
char   *FTPS_get_password (int shandle, int n);
int     FTPS_get_item (int shandle, int n, int *OUTPUT, int *OUTPUT, int *OUTPUT, int *OUTPUT);
