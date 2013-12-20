
/* How to use FTPSearch interface
 * ------------------------------
 * 1) Making new search
 * First you call FTPS_query() which returns search handle. Then
 * FTPS_search will perform the search itself storing results in
 * the 'filename' (which will be created/truncated if needed).
 *
 */

/* ARDP stuff
 * This part should not probably be used in applications.
 */

/* sets up `connection' to specified site, i.e. establishes information about where
 to send/receive packets. returns 0 if OK, -1 if error */
int ardp_open (char *hostname, int port);

/* destroys current `connection', i.e. free all resources associated with it */
void ardp_close (int sock);

/* sends data (NULL-terminated string) to server. returns -1 if error */
int ardp_send (int sock, char *s);

/* cancels request; returns -1 if error */
int ardp_cancel (int sock, int id);

/* receives data from server. returns length of malloc()ed buffer (s), or -1 if error.
 0 indicates that pending data were discarded and no useful input is available.
 don't free() the *s as it will be eventually freed by ardp_close() */
int ardp_receive (int sock, char **s);

/* returns whether further input is expected or not */
int ardp_sequence (void);

/* waits for data in the socket. also send confirmations when necessary. t is in
 seconds. returns 1 if data is available for reading, 0 if timeout, -1 if error */
int ardp_select (int sock, double t);

/* -------- FtpSearch stuff --------------------------------------- */

#include <time.h>

typedef struct
{
    int            rights;
    unsigned long  size;
    time_t         timestamp;
    char           *hostname;
    char           *pathname;
    char           *filename;
    char           *userid;
    char           *password;
    int            port;
}
ftps_search_results;

typedef struct
{
    ftps_search_results  *hits;
    int  n;
    char *request;
}
Results;

/* search type */
enum
{
    ST_EXACT=1,    /* exact: search-target must match filename exactly */
    ST_S_SUBSTR=2, /* substring: search-target must be a substring of filename */
    ST_S_REGEXP=3, /* regexp: search-target is a regular expression */
    ST_S_GLOB=4,   /* globbing: search-target is a shell wildcard expression */
    ST_M_SUBSTR=5, /* substring match */
    ST_M_REGEXP=6, /* regexp match */
    ST_M_GLOB=7,   /* globbing match */
    ST_NAVIGATE=8  /* navigation */
};

/* sort order (values are important!) */
enum {S_NAME=0, S_PATH=1, S_HOST=2, S_SIZE=3, S_TIME=4, S_DIR=5};

/* entry types: regular, directory, link, other */
enum {T_REG=1, T_DIR=2, T_LNK=3, T_OTH=4};

/* search by filetype */
enum {MT_FILE=0, MT_PICTURE=1, MT_SOUND=2, MT_MOVIE=3};

/* all functions return 0 on success. -1 typically indicates error unless
 mentioned otherwise */

/* sets search parameters (but does not start the search yet).

 char *target -- this is search target. function does not destroy it
 
 int maxhits -- max. number of hits to return (ignored for navigate type of
 requests)

 int mmtype -- multimedia type (use MT_FILE to execute regular
 search). ignored for navigation requests

 int req_type -- request type (ST_S_EXACT, ST_S_SUBSTR, ST_S_REGEXP, ST_S_GLOB,
 ST_M_EXACT, ST_M_SUBSTR, ST_M_REGEXP, ST_M_GLOB, NAVIGATE)

 int case_sen -- if nonzero be case sensitive, if 0 -- ignore case in target.
 Applies only to ST_S_SUBSTR, ST_M_SUBSTR. Should also apply to ST_S_GLOB,
 ST_M_GLOB (currently doesn't).

 int distrib_exclude -- whether to do filtering *BSD and Linux distributions.

 char *pathfilter -- filter for pathnames.
 See above explanation for colon-separated format. Use NULL if not needed

 char *domainfilter -- filter for domains.
 See above explanation for colon-separated format. Use NULL if not needed
 
 long start_date, long end_date -- min/max dates/times for files being searched.
 Values are really time_t, i.e. seconds elapsed since 1 Jan 1970 (Q: really?).
 Use 0 to ignore one or both of these criteria
 
 long min_size, long max_size -- min/max sizes for files begin searched.
 They are in bytes. Use 0 to ignore one or both of these criteria

 returns 'search handle' which can be lately used as 'index' in other calls
 */
int FTPS_query (char *target, int maxhits, int mmtype,
                int req_type, int case_sen, int distrib_exclude,
                char *path_inc, char *path_exc,
                char *dom_inc, char *dom_exc,
                long start_date, long end_date,
                long min_size, long max_size);

int parse_prospero_response (char *string, ftps_search_results **R);

/* does search. timeout is the max allowed time to spend searching, in
 seconds. returns: -1 if error (e.g., no parameters),
 -2 if timeout. 'no hits' is considered to be success. */
int FTPS_search (char *search_server, int search_port, int search_handle,
                 char *filename, int maxhits, double timeout);

/* loads stored prospero response from the file. returns request id */
int FTPS_load (char *filename);

/* sorts by several parameters. you can specify up to five ones and they will
 be used in order they appear in the argument list. use -1 to skip the parameter.
 example: FTPS_sort (S_TIME, S_HOST, -1, -1, -1) sorts by time, then hostname.
 first parameter (df) is whether to put directories first */
int FTPS_sort (int search_handle, int parm1, int parm2, int parm3, int parm4, int parm5);

/* frees all structures associated with search_handle. can be issued by  */
int FTPS_free (int search_handle);

int FTPS_num_results (int search_handle);

ftps_search_results *FTPS_get_handle (int search_handle);
char *FTPS_get_request (int search_handle);

int FTPS_set_num_results (int search_handle, int n1);

