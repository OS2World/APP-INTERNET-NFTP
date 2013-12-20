#ifndef _MANYCODE_H_INCLUDED_
#define _MANYCODE_H_INCLUDED_

/* we need it for size_t */
#include <sys/types.h>

#define MAX_CODEPAGE 1300

/* we need our own wchar_t as some compiler defines it as 16-bit value! */
typedef unsigned int Wchar_t;

enum
{
    EN_UNKNOWN   = 0,
    EN_ISO8859_1 = 850,
    EN_ALT       = 866,
    EN_KOI8R     = 878,
    EN_ISO       = 915,
    EN_UTF8      = 1208,
    EN_MAC       = 1283,
    EN_WINDOWS   = 1251,
};

/* possible values for guesser type:
 0 - utf8 + pure rcode,
 1 - utf8 + pure xcode,
 2 - utf8 + rcode + xcode,
 3 - asv trigram-based guesser (default) */
extern int guesser_type;
extern int guesser_babble;

/* returns guessed encoding of string 's' */
int rcd_guess (char *s);
int rcd_guess_xcode (char *s);

/* similar to rcd_guess() but optimized for texts of arbitrary length
 with mixed english/russian words */
int rus_encoding (char *s);

/* convert text to KOI8 and return codepage which was before converting */
int convert_to_koi8 (char *s);

/* translates string 's' in place from encoding 'from' to 'to'.
 'from' could be EN_UNKNOWN (the guess will be used). returns 0
 if success, and -1 in case of error (unable to guess, or wrong
 parameters). on error 's' is unmodified */
int rcd_translate (char *s, int from, int to);

/* replaces 'yo' to 'ye' in cyrillic KOI-8R texts. when 'len=0',
 string is considered NUL-terminated */
int replace_yo (char *s, int len);

/* converts from Russian encoding 'from' to KOI8 using
 straight tables (high performance!), without converting
 to Unicode first. Valid values for 'from':
 EN_ALT, EN_MAC, EN_ISO, EN_WINDOWS. On error (unknown
 encoding), -1 is returned. */
int ru_convert (char *s, int from);

/* converts to Russian encoding 'to' from KOI8 using
 straight tables (high performance!), without converting
 to Unicode first. Valid values for 'to':
 EN_ALT, EN_MAC, EN_ISO, EN_WINDOWS. On error (unknown
 encoding), -1 is returned. */
int ru_convert2 (char *s, int to);

/* returns name of the codepage (statically allocated string) */
char *ru_cp_name (int cp);

/* convert string in 1251 to lowercase */
void strlwr_1251 (char *s);

/* converts one wide character 'wc' to UTF-8 sequence. sequence is
 stored in 's'. returns number of bytes placed in 's'; 's' must
 be at least 6 bytes long to accomodate max. transform, or -1
 if error (invalid 'wc' is passed) */
int wctoutf8 (char *s, Wchar_t wc);

/* converts UTF-8 sequence into wide char 'p'. at most 'n' bytes
 can be considered. if 'n' is 0, strlen(s) is assumed. returns
 number of bytes from 's' used to obtain 'p', or -1 if error. */
int utf8towc (Wchar_t *p, char *s, size_t n);

/* gets 8-bit string in KOI8 and returns malloc()ed string in UTF-8 */
/*char *koi2utf (char *s);*/

/* gets UTF-8 string and returns malloc()ed string in KOI-8. All non-KOI8
 chars are replaced by '?' */
/*char *utf2koi (char *s);*/

/* gets 8-bit string in 'cp' and returns malloc()ed string in UTF-8 */
char *cp2utf (char *s, int cp);

/* gets UTF-8 string and returns malloc()ed string in 'cp'. All non-cp
 chars are replaced by '?' */
char *utf2cp (char *s, int cp);

/* converts one cp character to Unicode */
Wchar_t cp2w (unsigned char c, int codepage);

/* converts one Unicode char to cp */
unsigned char w2cp (Wchar_t w, int codepage);

/* tries to assign/load conversion table corresponding to 'cp'. returns:
 0 if found successfully, -1 if failed (in this case ISO8859-1 table
 is used instead */
int check_cp_table (int cp);

/* returns number of translation pairs read from file, or -1 if error.
 allocates and sets 'in' and 'out'. */
int ucm_parse (char *filename, int **in, Wchar_t **out);

/* this is a piece related to KOI8 processing */
#define	_U	0x01 /* uppercase letter */
#define	_L	0x02 /* lowercase letter */
#define	_N	0x04 /* digit */
#define	_S	0x08 /* space */
#define	_P	0x10 /* printable */
#define	_C	0x20 /* control char */
#define	_X	0x40 /* hexadecimal digit */
#define	_B	0x80 /* ? */

extern const unsigned char _koi8_ctype_[];
extern const unsigned char _koi8_lchars_[];
extern const unsigned char _koi8_uchars_[];
extern const unsigned char _koi8_ToUP_[];
extern const unsigned char _koi8_ToLO_[];

#define	koi8_isalnum(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_U|_L|_N)       )
#define	koi8_isalpha(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_U|_L)          )
#define	koi8_iscntrl(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_C)             )
#define	koi8_isdigit(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_N)             )
#define	koi8_isgraph(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_P|_U|_L|_N)    )
#define	koi8_islower(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_L)             )
#define	koi8_isprint(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_P|_U|_L|_N|_B) )
#define	koi8_ispunct(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_P)             )
#define	koi8_isspace(c)	 ((_koi8_ctype_)[(unsigned char)c] & (_S)             )
#define	koi8_isupper(c)  ((_koi8_ctype_)[(unsigned char)c] & (_U)             )
#define	koi8_isxdigit(c) ((_koi8_ctype_)[(unsigned char)c] & (_X)             )
#define	koi8_isascii(c)	 ((unsigned)(c) <= 0177)
#define	koi8_isblank(c)	 ((c) == '\t' || (c) == ' ')
#define	koi8_toascii(c)	((c) & 0177)
#define koi8_toupper(c) ((int) (_koi8_ToUP_[ (int)(unsigned char)(c) ]))
#define koi8_tolower(c) ((int) (_koi8_ToLO_[ (int)(unsigned char)(c) ]))

/*string manipulation functions*/
char *koi8_strlwr (char *string);
char *koi8_strupr (char *string);
char *koi8_strcasestr (const char *big, const char *little);

int koi8_strcmp (const char *string1, const char *string2);
int koi8_strncmp (const char *string1, const char *string2, size_t count);
/* "ABab" */

int koi8_strcasecmp (const char *string1, const char *string2);
int koi8_strncasecmp (const char *string1, const char *string2, size_t count);
/* "aABb" */

int koi8_strtitlecasecmp (const char *string1, const char *string2);
int koi8_strntitlecasecmp (const char *string1, const char *string2, size_t count);
/* "AaBb" */

char *koi8_strlwrdiacritics (char *string);
/* ³->å, £->Å */

/* functions/macros which need to be replaced:
 isalnum isalpha isascii isblank iscntrl isdigit isgraph islower isprint
 ispunct isspace isupper isxdigit toascii toupper tolower
 strcmp strncmp strcasecmp strncasecmp */
#ifdef KOI8_STRING_FUNCTIONS

#undef  isalnum
#define	isalnum(c)  koi8_isalnum(c)
#undef  isalpha
#define	isalpha(c)  koi8_isalpha(c)
#undef  isascii
#define	isascii(c)  koi8_isascii(c)
#undef  isblank
#define	isblank(c)  koi8_isblank(c)
#undef  iscntrl
#define	iscntrl(c)  koi8_iscntrl(c)
#undef  isdigit
#define	isdigit(c)  koi8_isdigit(c)
#undef  isgraph
#define	isgraph(c)  koi8_isgraph(c)
#undef  islower
#define	islower(c)  koi8_islower(c)
#undef  isprint
#define	isprint(c)  koi8_isprint(c)
#undef  ispunct
#define	ispunct(c)  koi8_ispunct(c)
#undef  isspace
#define	isspace(c)  koi8_isspace(c)
#undef  isupper
#define	isupper(c)  koi8_isupper(c)
#undef  isxdigit
#define	isxdigit(c) koi8_isxdigit(c)
#undef  toascii
#define	toascii(c)  koi8_toascii(c)
#undef  toupper
#define toupper(c)  koi8_toupper(c)
#undef  tolower
#define tolower(c)  koi8_tolower(c)
#undef strcmp
#define strcmp      koi8_strcmp
#undef strncmp
#define strncmp     koi8_strncmp
#undef strcasecmp
#define strcasecmp  koi8_strcasecmp
#undef strncasecmp
#define strncasecmp koi8_strncasecmp

#endif /* KOI8_STRING_FUNCTIONS */


/* this conditional will help to finc signed/unsigned
 errors with chars. Don't link or run with this option */
#ifdef TEST_SIGNED_CHAR_CTYPE

extern int ctype_table[];

#undef  isalnum
#define	isalnum(c)  ctype_table[c]
#undef  isalpha
#define	isalpha(c)  ctype_table[c]
#undef  isascii
#define	isascii(c)  ctype_table[c]
#undef  isblank
#define	isblank(c)  ctype_table[c]
#undef  iscntrl
#define	iscntrl(c)  ctype_table[c]
#undef  isdigit
#define	isdigit(c)  ctype_table[c]
#undef  isgraph
#define	isgraph(c)  ctype_table[c]
#undef  islower
#define	islower(c)  ctype_table[c]
#undef  isprint
#define	isprint(c)  ctype_table[c]
#undef  ispunct
#define	ispunct(c)  ctype_table[c]
#undef  isspace
#define	isspace(c)  ctype_table[c]
#undef  isupper
#define	isupper(c)  ctype_table[c]
#undef  isxdigit
#define	isxdigit(c) ctype_table[c]
#undef  toascii
#define	toascii(c)  ctype_table[c]
#undef  toupper
#define toupper(c)  ctype_table[c]
#undef  tolower
#define tolower(c)  ctype_table[c]

#endif

#endif /* _MANYCODE_H_INCLUDED_ */
