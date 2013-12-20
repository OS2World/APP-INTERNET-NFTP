#ifndef _ASVTOOLS_H_INCLUDED_
#define _ASVTOOLS_H_INCLUDED_

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <stdarg.h>

#if 1
#include <stdint.h>
#endif

#if 1
#include <fcntl.h>
#endif

#include <unistd.h>

#define BINMODE 0

/* C types definitions */

#if ! 1
typedef uint8_t u_int8_t;
#endif

#if ! 1
typedef uint16_t u_int16_t;
#endif

#if ! 1
typedef uint32_t u_int32_t;
#endif

#if ! 1
typedef uint64_t u_int64_t;
#endif


#if USE_OUR_REGEX_LIBRARY
   #define regoff_t   regoff1_t
   #define regex_t    regex1_t
   #define regmatch_t regmatch1_t
   
   #define	REG_BASIC	REG1_BASIC
   #define	REG_EXTENDED	REG1_EXTENDED
   #define	REG_ICASE	REG1_ICASE
   #define	REG_NOSUB	REG1_NOSUB
   #define	REG_NEWLINE	REG1_NEWLINE
   #define	REG_NOSPEC	REG1_NOSPEC
   #define	REG_PEND	REG1_PEND
   #define	REG_DUMP	REG1_DUMP
   
   #define	REG_NOMATCH    REG1_NOMATCH
   #define	REG_BADPAT     REG1_BADPAT
   #define	REG_ECOLLATE   REG1_ECOLLATE
   #define	REG_ECTYPE     REG1_ECTYPE
   #define	REG_EESCAPE    REG1_EESCAPE
   #define	REG_ESUBREG    REG1_ESUBREG
   #define	REG_EBRACK     REG1_EBRACK
   #define	REG_EPAREN     REG1_EPAREN
   #define	REG_EBRACE     REG1_EBRACE
   #define	REG_BADBR      REG1_BADBR
   #define	REG_ERANGE     REG1_ERANGE
   #define	REG_ESPACE     REG1_ESPACE
   #define	REG_BADRPT     REG1_BADRPT
   #define	REG_EMPTY      REG1_EMPTY
   #define	REG_ASSERT     REG1_ASSERT
   #define	REG_INVARG     REG1_INVARG
   #define	REG_ATOI       REG1_ATOI
   #define	REG_ITOA       REG1_ITOA
   
   #define	REG_NOTBOL     REG1_NOTBOL
   #define	REG_NOTEOL     REG1_NOTEOL
   #define	REG_STARTEND   REG1_STARTEND
   #define	REG_TRACE      REG1_TRACE
   #define	REG_LARGE      REG1_LARGE
   #define	REG_BACKR      REG1_BACKR
   
   #define regcomp   regcomp1
   #define regerror  regerror1
   #define regexec   regexec1
   #define regfree   regfree1
#else
   #include <regex.h>
#endif


#ifndef TRUE
    #define TRUE 1
#endif
#ifndef FALSE
    #define FALSE 0
#endif

#define split_to_digits(n,i0,i1,i2) {i0 = n/100; i1 = (n-i0*100)/10; i2 = (n-i0*100-i1*10);}
#define max1(a,b) (((a) > (b)) ? (a) : (b))
#define min1(a,b) (((a) < (b)) ? (a) : (b))
#define uc(p)  ( *( (unsigned char *) (p) ) )

#define URL_FLAG_FIREWALLED    0x0001    /* site is behind the firewall */
#define URL_FLAG_PASSIVE       0x0002    /* use passive mode */
#define URL_FLAG_RETRY         0x0004    /* retry indefinitely */
#define URL_FLAG_NOREGET       0x0008    /* reget does not work */
#define URL_FLAG_SKEY          0x0010    /* S/Key challenge is used; delay password prompt */

typedef struct
{
    char hostname[128];
    char userid[128];
    char password[32];
    char pathname[1024];
    char description[128];
    unsigned long ip;
    unsigned port;
    int  flags;
}
url_t;

extern FILE *tools_debug;

/*****************************************************************************
Henry Spencer's Regular Expression library, cleanly imported */

typedef int regoff1_t;

typedef struct
{
	int re_magic;
	size_t re_nsub;		/* number of parenthesized subexpressions */
	__const char *re_endp;	/* end pointer for REG_PEND */
	struct re1_guts *re_g;	/* none of your business :-) */
}
regex1_t;

typedef struct
{
	regoff1_t rm_so;		/* start of match */
	regoff1_t rm_eo;		/* end of match */
}
regmatch1_t;

/* regcomp() flags */
#define	REG1_BASIC	0000
#define	REG1_EXTENDED	0001
#define	REG1_ICASE	0002
#define	REG1_NOSUB	0004
#define	REG1_NEWLINE	0010
#define	REG1_NOSPEC	0020
#define	REG1_PEND	0040
#define	REG1_DUMP	0200

/* regerror() flags */
#define	REG1_NOMATCH	 1
#define	REG1_BADPAT	 2
#define	REG1_ECOLLATE	 3
#define	REG1_ECTYPE	 4
#define	REG1_EESCAPE	 5
#define	REG1_ESUBREG	 6
#define	REG1_EBRACK	 7
#define	REG1_EPAREN	 8
#define	REG1_EBRACE	 9
#define	REG1_BADBR	10
#define	REG1_ERANGE	11
#define	REG1_ESPACE	12
#define	REG1_BADRPT	13
#define	REG1_EMPTY	14
#define	REG1_ASSERT	15
#define	REG1_INVARG	16
#define	REG1_ATOI	255	/* convert name to number (!) */
#define	REG1_ITOA	0400	/* convert number to name (!) */

/* regexec() flags */
#define	REG1_NOTBOL	00001
#define	REG1_NOTEOL	00002
#define	REG1_STARTEND	00004
#define	REG1_TRACE	00400	/* tracing of execution */
#define	REG1_LARGE	01000	/* force large representation */
#define	REG1_BACKR	02000	/* force use of backref code */

int	regcomp1 (regex1_t *, const char *, int);
size_t	regerror1 (int, const regex1_t *, char *, size_t);
int	regexec1 (const regex1_t *, const char *, size_t, regmatch1_t [], int);
void	regfree1 (regex1_t *);

/*****************************************************************************
End of Henry Spencer's Regular Expression library */

/* -----------------------------------------------------------------------
 `str_shorten' shortens file name to FAT convention. This function does
 not check for existence of file with the same name; it's just an
 algorithm to reduce number of dots and symbols.
 parameter: s - original filename
 returns: statically allocated buffer with converted string, of NULL if failed
 -----------------------------------------------------------------------  */
char *str_shorten (char *s);

/* returns number of chars 'c' in string 's' */
int  str_numchars (char *s, char c);

/* removes chars from 'stray' at the tail of string 's' if present */
void str_strip (char *s, char *stray);

/* removes chars from 'stray' at the head and tail of string 's' if present */
void str_strip2 (char *s, char *stray);

/* copies `src' to `dest', reducing it to `dest_len' symbols */
void str_reduce (char *dest, char *src, int dest_len);

/* returns 1 if 'c' can be used in FAT filename, or 0 if not */
int  str_fatsafe (char c);

/* returns 1 if 'c' is vowel, and 0 if not */
int  str_isvowel (char c);

/* removes char at position 'p' from string 's'. \0 won't be removed */
void str_delete (char *s, char *p);

/* returns 0 if string 's' ends with 'tail', 1 if not */
int  str_tailcmp (char *s, char *tail);

/* returns 0 if string 's' starts with 'head', 1 if not */
int str_headcmp (char *s, char *head);

/* returns 0 if string 's' ends with 'tail', 1 if not.
 compare is not case-sensitive */
int  str_tailcmpi (char *s, char *tail);

/* makes string 's' one byte shorter */
void str_try_shorten (char *s);

/* returns last character in 's' belonging to set in 'accept';
 NULL if not found */
char *str_lastspn (char *s, char *accept);

/* pretty-printing for the whole number. returns pointer to statically
 allocated, NULL-terminated buffer of length 13. */
char *insert_commas (unsigned long number);

/* pretty-printing for the whole number. returns pointer to statically
 allocated, NULL-terminated buffer. Unlike insert_commas, this version
 does not pad front with spaces */
#define pretty insert_commas2
char *insert_commas2 (unsigned long number);

/* similar to insert_commas but using int64 input. static buffer
 is 26 bytes long. */
char *insert_commas3 (int64_t number);

/* similar to insert_commas2 but for 64bit integers */
#define pretty64 insert_commas4
char *insert_commas4 (int64_t number);

/* this one does not really inserts commas but just produces
 a string with 64-bit integer. leading spaces are skipped */
char *insert_commas5 (int64_t number);

#define printf64 insert_commas6
/* skips leading zeroes on 64-bit number */
char *insert_commas6 (int64_t number);

/* refine_buffer() breaks one long string into number of small ones
 at LF/CRs. returns the total length of resulting string */
int str_refine_buffer (char *buf);

/* returns static buffer containing local time in pretty format */
char *datetime (void);

/* pretty-printing for the date. returns pointer to statically
 allocated, NUL-terminated buffer. format: 31/12/2000 23:59:59 */
char *pretty_date (time_t t);

/* pretty-printing for the date only. returns pointer to statically
 allocated, NUL-terminated buffer. format: 31/12/2000 */
char *pretty_dateonly (time_t t);

/* pretty-printing for the time difference. returns pointer to
 statically allocated, NUL-terminated buffer. */
char *pretty_difftime (time_t t);

/* returns position at which two strings are different. compare is
 not case sensitive */
int str_imatch (char *s1, char *s2);

/* replaces all occurances of char `in' with char `out' in string `s' */
void str_translate (char *s, char in, char out);

/* very fast malloc without overhead and free(). makes a copy of string
 `s' and returns pointer to newly allocated place. this interface is totally
 different from chunk_() functions below */
char *chunk_add (char *s);

/* create a new set of memory chunks. returns an
 opaque pointer which must be later passed to chunk_* functions.
 'fragm_size' is a suggested size of the each chunk; use 0 for
 default value (1024KB). */
void *chunk_new (int fragm_size);

/* put 'len' bytes of 'data' into chunk set 'pch'. if 'len' == -1,
 use strlen() on 'data' to determine data size and add one byte for
 terminating NUL */
void *chunk_put (void *p, char *data, int len);

/* allocate 'len' bytes in the chunk set 'pch' */
void *chunk_alloc (void *p, int len);

/* frees all memory associated with given chunk set 'pch'. all pointers
 returned by chunk_put() become invalid. */
void chunk_free (void *p);

/* concatenates two strings putting one and only one slash between them */
char *str_cats (char *dest, char *src);

/* loads file into malloc()ed buffer. returns length of the file or -1
 if error occurs */
long load_file (char *filename, char **p);

/* writes buffer into file truncating existing one if necessary.
 returns bytes written or -1 if error occurs */
long dump_file (char *filename, char *p, long length);

/* copies file from dest to src. avoid large files. returns 0 on
 success, 1 on failure */
int copy_file (char *src, char *dest);

/* copy 'nb' bytes from 'in' to 'out', using 'mem' bytes as temp buffer.
 if 'buf' is not NULL, it is used as temp buffer containing 'mem' bytes.
 returns number of bytes copied */
int64_t copy_bytes (FILE *in, FILE *out, int64_t nb, int mem, char *buf);

/* some compilers mysteriously lack filelength(). this is a replacement */
unsigned long file_length (int fd);
int64_t f_length (char *fname);

/* extended version of mkdir. creates all intermediate directories if needed */
int make_subtree (char *s);

/* index1 - finds unescaped character `needle' in the string `haystack'
 (ignore those protected by '\' and inside ") */
char *str_index1 (char *haystack, char needle);

/* makes pathname look much, much better */
void sanify_pathname (char *s);

/* lowercases all chars in the string using `tolower' */
void str_lower (char *s);

/* strips CRs and LFs from the string's tail */
#define str_strip_nl(s) str_strip (s, "\n\r")

/* str_scopy is a safe version of strcpy. it does not copy past
 sizeof (dest), and after operation dest[sizeof(dest)-1] is always 0,
 thus ensuring that string is NULL-terminated. It is macro and
 has no return value */
#define str_scopy(d,s) (strncpy (d, s, sizeof(d)), d[sizeof(d)-1] = '\0')

/* second counter, as a double for easy operation */
extern int clock1_call_counter;
double clock1 (void);

/* wait for specified number of seconds */
void sleep1 (double interval);

/* strstr() in case insensitive flavour */
char *str_stristr (char *haystack, char *needle);

/* prints error message to stderr and exits */
void error1 (char *format, ...);

/* prints error message to fp and exits */
void error2 (FILE *fp, char *format, ...);
void error3 (FILE *fp, char *format, ...);

/* prints warning message to stderr */
void warning1 (char *format, ...);

/* prints warning message to fp */
void warning2 (FILE *fp, char *format, ...);
void warning3 (FILE *fp, char *format, ...);

/* returns malloc()ed buffer with description which is looked in
 NULL-terminated buffer `s'. name of the file is `filename', size is `size'.
 when no description is found, NULL is returned */
char *str_find_description (char *s, char *filename, unsigned long size);

/* breaks typical line of "aaa=bbb" kind to two components (name and value),
 both malloc()ed. line is unchanged. returns 0 if success, -1 if error
 ('=' not found in the string) */
int str_break_ini_line (char *line, char **var_name, char **var_value);

/* replaces all occurances of string s1 with with string s2 in the string s.
 returns number of changes made. recursive or not */
int str_replace (char *s, char *s1, char *s2, int recursive);

/* remove repeating sequences of '. .' */
void remove_dots (char *s);

/* replaces repeated sequences of whitespace chars with single space */
void remove_whitespace (char *s);

/* prints file on stdout */
void print_file (char *filename);

/* returns pointer to malloc()ed buffer holding URL-safe representation of
 string `src', i.e. with all suspicious chars replaced by %XX constructs */
char *hexify (char *src);

/* returns pointer to malloc()ed buffer holding URL-safe representation of
 string `src', i.e. with all suspicious chars replaced by %XX constructs */
char *hexify2 (char *src, char *unsafe);

/* converts URL-encoding (%20) representations into plain string.
 the conversion is done in-place */
void dehexify (char *s);

/* finds the token in `s' separated by `d', puts ending \0
 and returns pointer to token. advances `s' to point to first
 symbol of token. destructive */
char *str_sep1 (char **s, char d);

/* internal debug output routine */
void debug_tools (char *format, ...);

/* parses given 'url' and gives values to 's' elements.
 returns 0 when no errors, 1 if failed (empty URL)
 ftp://name:pw@hostname:6000/directory
 destructive on url */
int parse_url (char *url, url_t *s);

/* composes URL from given structure. returns malloc()ed buffer.
 when pass=1, write explicit password into it */
char *compose_url (url_t u, char *filename, int pass, int squidy);

/* composes URL from given structure. returns malloc()ed buffer.
 when pass=1, write explicit password into it. makes more 'safe'
 URLs suitable for feeding into command-line programs, not only WWW browsers */
char *compose_url2 (url_t u, char *filename, int pass);

/* initializes all fields of ftp URL to empty or reasonable values */
void init_url (url_t *u);

/* lowercases char; english only! */
char tolower1 (char c);

/* reads file and treates it as text: breaks into individual lines,
 returns pointer to these lines and number of them.
 to free memory, issue free() on first element of pointers array
 and then free pointers array:
 char  **L;
 if (load_textfile ("information.txt", &L) < 0) return;
 ...
 free (str[0]);
 free (L);
 return value: number of lines in the file, or -1 if error */
int load_textfile (char *filename,  char ***lines);

/* breaks text into lines. original text is not preserved,
 'lines' array is malloc()ed and its individual entries point
 to places in 's' */
int text2lines (char *s, char ***lines);

/* reads stdin into malloc()ed buffer. returns length of the buffer
 or -1 if error occurs */
int load_stdin (char **p);

/* writes file description into specified file.
 returns 0 on success and -1 if error occurs (cannot create description
 file, or cannot open it, or error while writing */
int write_description (char *filename, char *description, char *descfile);

/* converts month name into number. returns January when unknown.
 months start from 0 (January - 0) */
int txt2mon (char *m);

/* parses string representation of date, setting corresponding
 fields in the tm struct. string is not necessary needed length */
#define DATE_FMT_1  1  /* 27-SEP-1996 */
#define DATE_FMT_2  2  /* 27/09/96    */
#define DATE_FMT_3  3  /* 09/27/96    */
#define DATE_FMT_4  4  /* Sep 27 14:52 or Sep 27  1996 */
#define DATE_FMT_5  5  /* 96/09/27 */
#define DATE_FMT_6  6  /* Jul 17 02:08:00 1999 */
#define DATE_FMT_7  7  /* YYYYMMDDHHMMSS.sss */
#define DATE_FMT_8  8  /* 2000/01/18 */
#define DATE_FMT_9  9  /* Jul 17 02:08 1999 */
#define DATE_FMT_10 10 /* YYYY[[[[[MM]DD]HH]MM]SS] */
time_t parse_date_time (int fmt, char *ds, char *ts);

/* Due to unknown reasons, some systems have stricmp while others have
 strcasecmp. We don't want to depend on it */
int stricmp1 (char *s1, char *s2);

/* Due to unknown reasons, some systems have strnicmp while others have
 strncasecmp. We don't want to depend on it */
int strnicmp1 (char *s1, char *s2, int n);

/* tries to guess whether file is an index file containing descriptions */
int  isindexfile (char *name);

/* copies string `s' into newly allocated buffer (just like strdup), but uses
 buffer N bytes longer than exactly needed for the string s. Returns NULL when
 malloc() fails */
char *str_strdup1 (char *s, int N);

/* inserts char 'c' at position 'n' into string 's'. make sure 's' is long enough */
void str_insert (char *s, char c, int n);

/* does strncpy (dest, src, maxlength), then dest[maxlength-1] = 0. */
void str_copy (char *dest, char *src, int maxlength);

/* makes pathname look much, much better. returns malloc()ed string */
char *str_sanify (char *s);

/* converts file permissions from textual form (drwxr-xr--) to binary form as used
 in e.g. stat() call. string must be at least 10 symbols long. when errors occurs,
 0 is returned */
int perm_t2b (char *s);

/* converts file permissions from binary form to textual (rwxr-xr--).
 returns pointer to static, NULL-terminated buffer of 9 bytes long buffer */
char *perm_b2t (int rights);

/* convert hex character into decimal digit, i.e. '0'->0, ..., '9'->9, 'a'->10, ... */
int hex2dec (unsigned char x);
unsigned char dec2hex (int x);

/* pretty-printing for time (in seconds). returns statically allocated bytes of length 9. */
char *make_time (unsigned long time);

/* does BASE64 encoding of string 's' of length 'l'. If l == -1,
 then length is assumed to be determined by strlen (s). returns
 malloc()ed space holding base64 representation */
char *base64_encode (unsigned char *s, int len);

/* does BASE64 decoding of string 's'. result is placed into
 malloc()ed buffer and its length is assigned to len. returns
 0 if OK and -1 if failed (non-base64 chars in input). */
int base64_decode (unsigned char *s, char **decoded, int *len);

/* decomposes 's' into 'path' and 'name' components. takes care
 about drive letters if necessary. returns malloc()ed path and name;
 if 's' does not contain path components, 'path' is NULL */
void str_pathdcmp (char *s, char **path, char **name);

/* prepares to search for descriptions. 's' is the buffer where descriptions will be
 looked up; unchanged upon exit */
void str_parseindex (char *s);

/* returns malloc()ed buffer with description which is looked in strings 'index'.
 name of the file is `filename', size is `size'.
 when no description is found, NULL is returned */
char *str_finddesc (char *filename, unsigned long size);

/* frees internal structures used for descriptions lookup */
void str_freeindex (void);

/* counts occurence of 'pattern' in 's' */
int str_numstr (char *s, char *pattern);

/* joins two strings s1 and s2 and returns pointer to malloc()ed space
 which holds the result */
char *str_join (char *s1, char *s2);

/* appends s2 to s1 in malloc()ed space, frees s1, returns pointer to new
 string */
char *str_append (char *s1, char *s2);

/* returns malloc()ed concatenation of s1 and s2, and slash between */
char *str_sjoin (char *s1, char *s2);

/* breaks given line 's' using 'sep' as separators. creates an array of
 string pointers in 'L' and returns number of them. each string in the
 result is malloc()ed. this function always returns at least one item
 in L (perhaps empty). */
int str_parseline (char *s, char ***L, char sep);

/* joins n strings from array L putting sep between them.
 returns malloc()ed buffer; separators aren't placed at the
 beginning and the end of the resulting string */
char *str_mjoin (char **L, int n, char sep);

/* joins n strings from array L putting 'sep' between them.
 returns malloc()ed buffer; separators aren't placed at the
 beginning and the end of the resulting string */
char *str_msjoin (char **L, int n, char *sep);

/* Convert a string to a quad integer */
int64_t strtoq1 (const char *nptr, char **endptr, int base);

/* converts binary string into its hexadecimal representation. returns
pointer to malloc()ed buffer */
char *bin2hex (unsigned char *bin, int len);

/* converts string of hexadecimal chars into string. assigns bin and len.
length of source must be even. */
void hex2bin (char *hx, char **bin, int *len);

/* converts time broken representation into number of seconds. no
 timezone conversion is assumed */
time_t timegm1 (struct tm *gmtimein);

time_t local2gm (time_t t1);
time_t gm2local (time_t t1);

/* tries to determine whether file is text (not binary)
 file. set probe_size to -1 to use default. returns 0
 if file is binary, 1 if file is text, and -1 on error
 (file does not exist or has zero length) */
int is_textfile (char *filename, int probe_size);

/* print a string 's' to stream 'fp' breaking it into
 several lines, each not longer than maxlen and
 ending with backslash */
int flongprint (FILE *fp, char *s, int maxlen);
int longprint (int fd, char *s, int maxlen);

/* breaks given line 's' into words. creates an array of
 string pointers in 'L' and returns number of them. each string in the
 result is a pointer to a place in 's'. original 's' is not preserved.
 sep_class sets what separators between words to assume:
 WSEP_NONE:   don't split the line, consider as one word
 WSEP_TEXT:   all chars are separators except letters and digits
 WSEP_SPACES: spaces are separators
 WSEP_COMMAS: commas are separators
 */
enum {WSEP_NONE, WSEP_SPACES, WSEP_TEXT, WSEP_WILDCARD, WSEP_COMMAS,
WSEP_PERIOD, WSEP_SEMICOLON, WSEP_VBAR, WSEP_LETTERS, WSEP_COLON};
int str_words (char *s, char ***L, int sep_class);

/* breaks given line 's' into words. creates an array of
 string pointers in 'L' and returns number of them. each string in the
 result is a pointer to a place in 's'. original 's' is not preserved.
 sep is a separator between words */
int str_split (char *s, char ***L, char sep);

/* sorts binary file using partial sort and merge (useful for large sorts).
 * Parameters:
 * fh_in, fh_out -- must be preopened handles for input/output files;
 * nmemb, size -- number of elements in input stream and size (in bytes)
 *       of each element, just like in qsort();
 * compar -- function to compare elements, like in qsort();
 * sort1 -- function which will be used for memory-based sort (specify
 *       NULL to use libc's qsort());
 * msize -- max. available memory for sorting, in bytes
 * tmp_path -- directory where temporary files will be written. when
 *       NULL is specified, system default is used (NULL DOES NOT WORK YET)
 */
int fsort (int fh_in, int fh_out, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *),
           void (*sort1)(void *base, size_t nmemb, size_t size, int (*compar1)(const void *, const void *)),
           int msize, char *tmp_path);

/* removes repetitive elements from binary file 
 * Parameters:
 * fh_in, fh_out -- must be preopened handles for input/output files;
 * nmemb, size -- number of elements in input stream and size (in bytes)
 *       of each element, just like in qsort();
 * compar -- function to compare elements, like in qsort();
 */
int funiq (int fh_in, int fh_out, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

/* deletes identical entries from the list; returns the final number of entries.
 arguments are the same as for qsort(). does not move discarded elements
 to the tail of th array so it is faster than uniq but cannot be
 used when elements contain malloc()ed memory, or leaks will occur. */
int uniq (void *array, size_t nmemb, size_t size, int compare(__const__ void *, __const__ void *));

/* deletes identical entries from the list; returns the final number of entries.
 arguments are the same as for qsort(). discarded elements are placed
 at the tail of the array. */
int uniq2 (void *array, size_t nmem, size_t smem, int compare(__const__ void *, __const__ void *));

/* merges n arrays into one array which is either intersection
 (op=1) or union (op=2) of the original arrays 'bases'. all
 operations are done in memory. result is malloc()ed, and
 its length (or -1 if error) is returned. nmembs[i] is a number
 of elements in bases[i], 'size' is a size (in bytes) of each
 element */
int merge (int op, int n, void **bases, int *nmembs, size_t size,
           void **result, int (*compar)(const void *, const void *));

/* partial sort. sorts 'array' using 'compare' but instead of
 sorting entire array it will only put 'limit' first least
 elements. the rest of an array is left in arbitrary order */
int psort (void *array, size_t nmemb, size_t size, int limit,
           int compare(__const__ void *, __const__ void *));

/* partial sort/uniq. sorts 'array' using 'compare' but instead of
 sorting entire array it will only put 'limit' first smallest
 elements. the rest of an array is left in arbitrary order */
int pusort (void *array, size_t nmemb, size_t size, int limit,
           int compare(__const__ void *, __const__ void *));
            
/* partial sort. sorts 'array' using 'compare' but instead of
 * sorting entire array it will only put not more than 'limit'
 * first smallest elements. duplicated elements are discarded.
 * returns number of elements in sorted subarray */
int psort1 (void *array, size_t nmemb, size_t size, int limit,
            int compare(__const__ void *, __const__ void *));
            
/* this function will try to bracket left and right elements in 'array'
 which bracket 'key', i.e. 'left' <= 'key' <= 'right' according 'compare'
 function. 'nmemb' and 'size' have the same meaning as in bsearch(). returns:
 0 if key is bracketed; 1 if key is found in the array L and R
 show all elements which are equal to key; -1 if key value is smaller than
 smallest (left) element, -2 if key value is larger than largest (right)
 element, -3 if array is empty */
int bracket (const void *key, void *array, size_t nmemb, size_t size,
             int compare(__const__ void *, __const__ void *),
             int *left, int *right);

/* sorts text file fn1, writes result to fn2. compare() is a function to
 compare pointers to strings, like cmp_str */
int sortfile (char *fn1, char *fn2,
              int compare(__const__ void *, __const__ void *), int msize,
              char *temp_path);

/* adds 'elem' to 'array' if 'elem' is smaller than largest element in
 * the array. returns 0 if element is skipped, 1 if added, negative
 * value on error */
int accumulate (void *array, void *elem, size_t size, 
                size_t *nmemb, size_t max_nmemb,
                int compare(__const__ void *, __const__ void *));

void sort_accumulated (void *array, size_t size, size_t nmemb,
                       int compare(__const__ void *, __const__ void *));

/* a system for reading text lines from the file using large memory
 * cache */

typedef struct
{
    FILE *fp;
    int  msize, nb, ns;
    char *buf;
}
linebuf_t;

/* returns 0 on success, -1 on error (not enough memory, can't read),
 * 1 if input stream is empty */
int linebuf_init (linebuf_t *lb, FILE *fp, int msize);

/* returns 0 on success ('*s' points to next line extracted from stream,
 * don't free() it), -1 on error, 1 when input stream is empty */
int linebuf_nextline (linebuf_t *lb, char **s);

/* returns 0 on success, -1 on error */
int linebuf_close (linebuf_t *lb);

/* returns 1 when 's' is correct Unix listing entry */
int is_unix_file_entry (char *s);

/* system-independent strptime() */
char *str_ptime(char *buf, char *fmt, struct tm *tm);

/* retrieves date from HTTP time string */
time_t http_getdate (char *datestring);

/* normalize URL: delete 'http://' if present, delete port
 number if default. delete slash at the end of the URL if
 present. replace backslashes with forward slashes. conversion
 is done in-place. */
int normalize_url (char *url);

/* normalize URL: delete '*://' if present, delete port
 number if default. replaces repetitive slashes with one. conversion
 is done in-place. returns 0 on success, and -1 on malformed and
 non-HTTP URLs */
int normalize_url2 (char *url);

/* replaces sequences of multiple isspace() chars with single space.
 returns new length of the string */
int str_compress_spaces (char *s);

#if 0 == 0
/* convert string to lower case using current locale */
char *strlwr (char *s);
#endif

#if 0 == 0
/* convert string to lower case using current locale */
char *strupr (char *s);
#endif

/* returns concentration of binary chars in the
 input string 's' of length 'nbytes'. in other words
 when '0.01' is returned then 1% of the input is binary. */
float binchars (char *s, int nbytes);

/* returns concentration of nontext chars in the
 input string 's' of length 'nbytes'. in other words
 when '0.01' is returned then 1% of the input is not a text (i.e.
 numbers, spaces etc.) 'Text chars' are: A-Z, symbols from high half
 of ASCII table. */
float nontextchars (char *s, int nbytes);

/* case insensitive strstr */
char *str_casestr (char *str, char *substr);

/* read file line-by-line and put every line into an array
 of malloc()ed strings. ignore empty strings and strings
 starting with '#'. return this array, NULL-terminated.
 returns NULL on error (file not found). every line in the
 returned array is malloc()ed. */
char **read_list (char *fname);

/*******************************************************
 check 'str' against NULL-terminated exclusion list 'excl'.
 if first symbol of exclusion pattern is ^, treat as
 exclude pattern, otherwise treat as include pattern.
 returns '1' if item is to be processed, and '0' if ignored.
 result depends on the order of pattern entries! this version
 supports pattern groups (lines beginning with '+' are treated
 as separators) */
int check_list (char **excl, char *str);

enum
{
    RULETYPE_COMPLEX=0,       /* contains wildcards in non-last position */
    RULETYPE_MATCH=1,         /* exact match (no wildcards) */
    RULETYPE_SIMPLE=2,        /* only wildcard is at the end */
    RULETYPE_COMPLEX_NEG=3,   /* contains wildcards in non-last position */
    RULETYPE_MATCH_NEG=4,     /* exact match (no wildcards) */
    RULETYPE_SIMPLE_NEG=5,    /* only wildcard is at the end */
    RULETYPE_SEPARATOR=6,     /* this is not actually a rule */
    RULETYPE_SUBCATS=7        /* list of subcategories */
};

typedef struct
{
    char *rule;
    int  category;
    char type;
    unsigned char len;
}
catrule_t;

typedef struct
{
    int n, ncats;
    catrule_t *rules;
    int n_simple, *simple, shortest;
    int n_complex, *complex;
}
fastlist_t;

/* prepare list for fast lookups */
fastlist_t *prepare_fastlist (char **list);

/* similar to check_list() but significantly faster on large rule lists */
int check_fastlist (fastlist_t *fl, char *str);

/* returns list of categories assigned to given URL. uses fast
 representation */
int fast_category_check (fastlist_t *fl, char *str, int **categories);

/*******************************************************
 check 'str' against exclusion list 'excl' which has 'n'
 elements (see check_list). pattern groups are NOT supported.
 returns '1' if 'str' is included, and '0' if not. */
int check_nlist (char **excl, int n, char *str);

/* pattern matching. */
typedef struct
{
    struct
    {
        char    type;
        char    *patt;
        regex1_t rx;
    }
    *patterns;
    int  np;
}
pattern_set;

int load_pattern_set (char *set_name, pattern_set *ps);
int check_pattern (char *name, pattern_set *ps);

typedef struct
{
    char *name;
    char *value;
}
header_line_t;

/* parses HTTP-style response in buffer 's'. 'headers' become filled with
 header information, n_headers will contain number of headers returned,
 'body' will point at the start of the actual response body. Original
 buffer 's' is not preserved, 'headers' itself is malloc()ed but
 (*headers)[i].{name|value} are not, they are pointers into 's'. returns
 responce code or negative value on error. First line of the response
 (with a code) is returned in (*headers)[0].value. */
int http_parse (char *s, int length, header_line_t **headers, int *n_headers, char **body);

/* compare strings */
int cmp_str (const void *e1, const void *e2);

/* compare lengths of strings */
int cmp_strlen (const void *e1, const void *e2);

/* compare integers */
int cmp_integers (const void *e1, const void *e2);

/* compare integers disregarding their sign */
int cmp_absintegers (const void *e1, const void *e2);

/* compare unsigned integers */
int cmp_unsigned_integers (const void *e1, const void *e2);

/* compare doubles */   
int cmp_doubles (const void *e1, const void *e2);

/* prints binary input to stdout, replacing non-printable chars
 with [0x12] equivalents */
void print_binary (char *s, int len);

/* prints bytes bit-by-bit */
void fprint_bits (FILE *fp, char *s, int len);

/* integer values for 'text/html' and 'text/plain'. valid
 after call to mime_load(). */
extern int mime_HTML, mime_PLAIN;

/* loads MIME database from given file. a good example
 of such file is 'mime.types' supplied with Apache. */
int mime_load (char *filename);

/* unload mimetype database, free memory */
int mime_unload (void);

/* return an integer corresponding to 'type'. returns -2 if
 MIME database was not initialized, -1 if 'type' is not found. */
int mime_name2num (char *type);

/* returns a pointer to the MIME type string corresponding to
 'num'. NULL is returned when MIME database was not initialized
 or 'num' is outside of range. */
char *mime_num2name (int num);

/* returns number of MIME types in the initialized database.
 negative values indicate that database was not initialized. */
int mime_ntypes (void);

/* return codes:
 -0 - success
 -1 - error loading
 -2 - invalid contents
  */
int load_ext_filters (char *fname);
int unload_ext_filters (void);

/* fnmatch() implementation, with corresponding flags. does not have locale
support */
#define FNM1_NOMATCH     1       /* Match failed. */
#define FNM1_NOESCAPE    0x01    /* Disable backslash escaping. */
#define FNM1_PATHNAME    0x02    /* Slash must be matched by slash. */
#define FNM1_PERIOD      0x04    /* Period must be matched by period. */
#define FNM1_LEADING_DIR 0x08    /* Ignore /<tail> after Imatch. */
#define FNM1_CASEFOLD    0x10    /* Case insensitive search. */
int fnmatch1 (const char *pattern, const char *string, int flags);

/* returns TRUE if 's' passes through extension filters
 or FALSE if it gets excluded */
int check_ext_filters (char *s);

/* opens file getting a lock on it. returns -1 if open
 fails or file descriptor on success. */
int openlock (char *name, int flags, int mode);

/* a version of snprintf. uses native snprintf when available and surrogate
 when not */
int snprintf1 (char *str, size_t size, const char *format, ...);
int vsnprintf1 (char *str, size_t size, const char *format, va_list args);

/* utility functions. all of them exit on error printing message
 to stderr except yunlink() which ignores errors */
int   xunlink (char *format, ...);
int   yunlink (char *format, ...);
int   xlseek (int fh, off_t offset);
int   xcreate (char *format, ...);
int   xopen (char *format, ...);
int   xwrite (int fd, void *buffer, int size);
int   xwrite_str (int fd, char *s);
int   xwrite_str_array (int fd, char **s, int n);
int   xread  (int fd, void *buffer, int size);
char *xread_str (int fd);
int   xread_str_array (int fd, char ***s);

/* xfread/xfwrite return number of bytes read/written.
 * read can return less bytes than requested; this not an error */
FILE *xfopen (char *format, char *mode, ...);
int xfclose (FILE *fp);
int xfwrite (void *buffer, int nb, FILE *fp);
int xfread (void *buffer, int nb, FILE *fp);

/* #define arraycheck(a,na,n,t)
 {
    if ((n) == (na)
            if (nfm_a == nfm)
            {
                nfm_a *= 2;
                fm = xrealloc (fm, sizeof (char *) * nfm_a);
            }
*/

#ifdef DMALLOC
   #define xmalloc malloc
   #define xrealloc realloc
   #define xfree free
   #define xstrdup strdup
#else
   void *xmalloc (size_t n);
   void *xrealloc (void *ptr, size_t n);
   void xfree (void *ptr);
   char *xstrdup(char *str);
#endif
char *xnstrdup(char *str, int len);

void *xmmap (unsigned long *len, char *format, ...);
void  xmunmap (void *addr, unsigned long len);

/* ---------------------------------------------------------------------
 simple configuration manager
 ----------------------------------------------------------------------- */

int   cfg_write (int group, char *configfile);
int   cfg_read (int group, char *configfile);
void  cfg_destroy (int group);

int   cfg_set_integer (int group, char *section, char *name, int value);
int   cfg_set_string (int group, char *section, char *name, char *value);
int   cfg_set_boolean (int group, char *section, char *name, int value);
int   cfg_set_float (int group, char *section, char *name, double value);
int   cfg_set_64bit (int group, char *section, char *name, int64_t value);

int   cfg_get_integer (int group, char *section, char *name); /* returns 0 on error */
char *cfg_get_string (int group, char *section, char *name);  /* returns "" on error */
int   cfg_get_boolean (int group, char *section, char *name); /* returns FALSE on error */
double cfg_get_float (int group, char *section, char *name); /* returns 0.0 on error */
int64_t cfg_get_64bit (int group, char *section, char *name);
       
/* functions return TRUE if variable exists and has indicated type, otherwise FALSE */

int   cfg_check_integer (int group, char *section, char *name);
int   cfg_check_string (int group, char *section, char *name);
int   cfg_check_boolean (int group, char *section, char *name);
int   cfg_check_float (int group, char *section, char *name);
int   cfg_check_64bit (int group, char *section, char *name);

/* functions return 0 on success and -1 if error */
int  infLoad (char *filename);
void infFree (void);
int  infGetInteger (char *section, char *variable, int *value);
int  infGetString (char *section, char *variable, char **value);
int  infGetBoolean (char *section, char *variable, int *value);
int  infGetHexbyte (char *section, char *variable, char *value);
int  infGetFloat (char *section, char *variable, float *value);
int  infGetDouble (char *section, char *variable, double *value);
/* inf iterator by teodor, infIterate returns name (should not changed!!!) */
void   infStartIteratorSection();
char*  infIterateSection();
void   infStartIteratorVariable();
/* option is xstrdup'ed */
char*  infIterateVariable(char *section, char **option);


char  *xlf_escape (char *s);
char  *xlf_unescape (char *s);
int    xlf_need_escape (char c);

char *country_name (int c);
char *country_abbrev (int c);
int find_country (int country);
int find_abbrev (char *abbrev);
int what_country (char *hostname);

/* connect to site/port */
void Set_TCPIP_timeout (double dt);

/* connect to IPv4 address/port */
int ConnectIP (unsigned long IP, int port);

int Connect (char *hostname, int port);
int Close (int sock);
int Send (int sock, char *buf);
char *Recv (int sock);
int XSend (int sock, char *s, ...);

/* read binary data from socket until exhaustion.
 returns number of bytes read, or negative value on
 error. does not close socket. 'buf' is returned,
 malloc()ed (and NUL-terminated, but NUL byte
 is not included into the returned byte count).
 when 0 is returned, 'buf' is not allocated. */
int BRecv (int sock, char **buf);

/* Integer cache read/write package */
enum {IC_PUT, IC_SEEK, IC_SET, IC_FLUSH, IC_FREE};

typedef struct 
{
    int membabble, buffersize, sw, bs;
    double iotime;
}
intcacheparms_t;

extern intcacheparms_t intcache;

int get64_integer (int fh, int64_t *num);
int get_integer (int fh, unsigned int *num);
int nget_integer (int N, int n, int fh, unsigned int *num);
int put64_integer (int fh, int64_t num);
int put_integer (int mode, int fh, unsigned int docid, int64_t *loc);
int nput_integer (int mode, int N, int n, int fh, unsigned int docid, int64_t *loc);

/* lockfile management */

/* returns 0 on success, 1 if file already exists and process is running
 and -1 on error */
int pidfile_create (char *pidfile, pid_t pid);

/* returns 0 on success, 1 if file does not exists and -1 on error */
int pidfile_remove (char *pidfile);

/* returns 0 if pidfile does not exist; 1 if pidfile exists and process is
 running; 2 if pidfile exists and process is not running */
int pidfile_check (char *pidfile);

/* document id range */
typedef struct
{
    int s_id, e_id; /* start id, end id */
}
id_range_t;

/* this is a set of id ranges */
typedef struct
{
    int         nr; /* number of ranges in 'ir' array */
    id_range_t *ir; /* id ranges. NULL when nr=0 */
}
rangeset_t;

/* performs intersection of two range lists: 'out' contains only
 overlapping portions of lists */
int ranges_intersect (rangeset_t *in1, rangeset_t *in2, rangeset_t *out);

/* performs merge of two range lists: 'out' contains ranges combined
 from both lists */
int ranges_merge (rangeset_t *in1, rangeset_t *in2, rangeset_t *out);

/* compares id_range_t values according s_id */
int cmp_range (const void *e1, const void *e2);

/* combines list of ranges into increasing, non-overlapping list of
 ranges */
int merge_ranges (int nr, id_range_t *ir);

/* A simple stack implementation */
int stack_create (int n, int size);
int stack_destroy (void);
int stack_push (void *item);
int stack_pop (void *item);
int stack_peek (void *item);

/* cuts off string tail to max. 'limit' chars and pads with '...' */
void str_cutoff (char *s, int limit);

/* Byte-aligned, variable-byte compression for integers. */

/* encodes integer n into buffer (buffer length must be at least 5 bytes),
 returns number of bytes used in the buffer */
int vby_encode (char *buf, u_int32_t n);

/* encodes 64 bit integer n into buffer (buffer length must be at
 least 10 bytes), returns number of bytes used in the buffer */
int vby_encode64 (char *buf, u_int64_t n);

/* decodes integer n from buffer (buffer length must be at least 5 bytes),
 returns number of bytes used from the buffer */
int vby_decode (char *buf, u_int32_t *n);

/* decodes 64 bit integer n from buffer (buffer length must be at
 least 10 bytes), returns number of bytes used from the buffer */
int vby_decode64 (char *buf, u_int64_t *n);

/* skips integer from buffer (buffer length must be at least 5 bytes),
 returns number of bytes skipped from the buffer */
int vby_skip (char *buf);

/* functions return number of bytes read/written/scanned and 0 in case
of error */
unsigned int vby_fread (FILE* fp, u_int32_t *n);
unsigned int vby_fwrite (FILE* fp, u_int32_t n);
unsigned int vby_fscan (FILE* fp);
unsigned int vby_len (u_int32_t n);

int vby_encode_s (char *buf, char *s);
int vby_decode_s (char *buf, char **s);

/* HTTP-style header cache. */

/* initialize header/response cache. 'code' and 'errmsg' are default
 code/message which will be sent if hdr_set_response() was not called
 before hdr_send() */
int hdr_init (int code, char *errmsg);

/* set response code and text */
int hdr_set_response (int code, char *format, ...);

/* add a header field */
int hdr_add (char *format, ...);

/* add header field without any processing */
int hdr_addraw (char *s);

/* send entire HTTP-style header to file stream fp */
int hdr_send (FILE *fp);

/* send entire HTTP-style header to socket s */
int hdr_write (int s);

/* read HTTP request from socket. 'headers' get filled with 'n_headers' items,
 'body' (not NULL if present) contains POST data. request is contained
 in headers[0].name. returns 0 on success, negative value on error. to
 free memory allocated by this call issue free() on every headers[i].name
 and headers itself; don't free headers[i].value, it is not malloc()ed but
 just a pointer into space pointed by headers[i].name */
int http_read_request (int sock, header_line_t **headers, int *n_headers,
                       char **body, int *n_body);

/* --------------------------------------------------------------------
 returns 0 on success, 1 if URL is invalid, 2 if not HTTP URL,
 and -1 on errors. 'hostname'
 and 'pathname' are malloc()ed or NULL if not in 'url'.
 Generic URL can look like (spaces are for readability!):
 [protocol:[//]] [username[:password]@] [hostname] [:port] [pathname]
 */
int parse_http_url (char *url, char **hostname, int *port, char **pathname);

/* returns TRUE is 's' looks like valid FDQN hostname and FALSE if not */
int valid_hostname (char *s);

/* returns non-zero when path is invalid */
int refine_pathname (char *path);

/* returns 0 on success, -1 on error and 1 if link is invalid, 2 if not HTTP.
 algorithm:
 1. parse 'url' into 'hostname', 'portnum', 'pathname'
 2. if 'hostname' is valid (not NULL) 'url' is complete, i.e. contains
 all parts (host, port, path). we just pass along results of the parse_url
 3. 'hostname' is NULL: 'url' is partial. partial url cannot contain port
 number. it can only contain path which can be relative or absolute. if
 'pathname' is NULL, this is an error (cannot happen).
 4. if 'pathname' starts with '/' (absolute pathname) we use it.
 5. cut base_pathname '?' (remove script parameters), locate last slash
 and join 'pathname' after it.
 */
int join_links (char *base_hostname, int base_port,
                char *base_pathname, char *url,
                char **host, int *port, char **path);

/* creates a set of output buffers in memory; nb is a size of one chunk.
 if n is zero or negative, 1MB is used. returns membuf id on success,
 negative value on error */
int membuf_create (int nb);

/* destroy corresponding membuf */
int membuf_close (int mbid);

/* put a NUL-terminated string into membuf. */
int membuf_put (int mbid, char *s);

/* put a character in membuf */
int membuf_putc (int mbid, char c);

/* dump membuf contents into file pointed by descriptor 'fd' */
int membuf_write (int mbid, int fd);

/* printf-like function for membuf_* interface */
int membuf_printf (int mbid, char *fmt, ...);

/* gather all buffers into one big malloc()ed string */
char *membuf_accumulate (int mbid);

/* basic heap operations */
void *heap_siftup (char *root, unsigned int element,
                   unsigned int size,
                   int (*cmp)(const void *one, const void *two));
                   
void *heap_siftdown (char *element, char *endel,
                     unsigned int size, unsigned int diff,
                     int (*cmp)(const void *one, const void *two));

/* 'x' in 'pwr' power */
double ipow (double x, int pwr);

/* reads one column (numbered 'ncol', starting from 1) from file
 'fname' and puts it into allocated 'data'. returns number of points read
 (length of 'data' array), or negative values on errors: -1 -- error reading
 input; -2 -- ncol is too big */
int read_column (char *fname, int ncol, double **data);

int read_datafile (char *fname, double ***datafile, int *ncols);

void drop_datafile (char *fname);

int read_one_column (char *arg, double **data);

int read_two_columns (char *arg, double **data1, double **data2);

int read_n_columns (char *arg, int *n, double ***data);

int interpolate_l (double *x1, double *y1, int n1, double *x2, double *y2, int n2);

#endif
