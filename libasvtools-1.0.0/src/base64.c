#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asvtools.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static unsigned char Base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static unsigned char Pad64 = '=';

/* does BASE64 encoding of string 's' of length 'l'. If l == -1,
 then length is assumed to be determined by strlen (s). returns
 malloc()ed space holding base64 representation */
char *base64_encode (unsigned char *s, int len)
{
    int   nl, i;
    char  *es, *p;
    
    if (len == -1) len = strlen ((char *)s);
    nl = len*4/3 + 8;

    es = malloc (nl);
    p = es;
    
    for (i=0; i<=len-3; i+=3)
    {
        *p++ = Base64[  s[i  ]         >> 2];
        *p++ = Base64[((s[i  ] & 0x03) << 4) | (s[i+1] >> 4)];
        *p++ = Base64[((s[i+1] & 0x0f) << 2) | (s[i+2] >> 6)];
        *p++ = Base64[  s[i+2] & 0x3f];
    }
    
    if (len-i == 2)
    {
        *p++ = Base64[  s[i]           >> 2];
        *p++ = Base64[((s[i  ] & 0x03) << 4) | ((s[i+1] & 0xf0) >> 4)];
        *p++ = Base64[((s[i+1] & 0x0f) << 2)];
        *p++ = Pad64;
    }
    else if (len-i == 1)
    {
        *p++ = Base64[ s[i] >> 2];
        *p++ = Base64[(s[i] & 0x03) << 4];
        *p++ = Pad64;
        *p++ = Pad64;
    }
    *p = '\0';

    return es;
}

/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */
/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
 */
/*
int
b64_pton(src, target, targsize)
	char const *src;
	u_char *target;
        size_t targsize;
        */

/* does BASE64 decoding of string 's'. result is placed into
 malloc()ed buffer and its length is assigned to len. returns
 0 if OK and -1 if failed (non-base64 chars in input). */
int base64_decode (unsigned char *s, char **decoded, int *len)
{
    int    tarindex, state, ch;
    unsigned char   *pos;

    *decoded = malloc (strlen ((char *)s)*3/4 + 8);
    state = 0;
    tarindex = 0;

    while ((ch = *s++) != '\0')
    {
        if (isspace(ch))	/* Skip whitespace anywhere. */
            continue;

        if (ch == Pad64)
            break;

        pos = (unsigned char *)strchr ((char *)Base64, ch);
        if (pos == 0) 		/* A non-base64 character. */
            return (-1);

        switch (state) {
        case 0:
            (*decoded)[tarindex] = (pos - Base64) << 2;
            state = 1;
            break;
        case 1:
            (*decoded)[tarindex] |= (pos - Base64) >> 4;
            (*decoded)[tarindex+1] = ((pos - Base64) & 0x0f) << 4;
            tarindex++;
            state = 2;
            break;
        case 2:
            (*decoded)[tarindex] |=  (pos - Base64) >> 2;
            (*decoded)[tarindex+1] = ((pos - Base64) & 0x03) << 6;
            tarindex++;
            state = 3;
            break;
        case 3:
            (*decoded)[tarindex] |= (pos - Base64);
            tarindex++;
            state = 0;
            break;
        }
    }

    /*
     * We are done decoding Base-64 chars.  Let's see if we ended
     * on a byte boundary, and/or with erroneous trailing characters.
     */

    if (ch == Pad64) {		/* We got a pad char. */
        ch = *s++;		/* Skip it, get next. */
        switch (state) {
        case 0:		/* Invalid = in first position */
        case 1:		/* Invalid = in second position */
            return (-1);

        case 2:		/* Valid, means one byte of info */
            /* Skip any number of spaces. */
            for ((void)NULL; ch != '\0'; ch = *s++)
                if (!isspace(ch))
                    break;
            /* Make sure there is another trailing = sign. */
            if (ch != Pad64)
                return (-1);
            ch = *s++;		/* Skip the = */
            /* Fall through to "single trailing =" case. */
            /* FALLTHROUGH */

        case 3:		/* Valid, means two bytes of info */
            /*
             * We know this char is an =.  Is there anything but
			 * whitespace after it?
                         */
            for (; ch != '\0'; ch = *s++)
                if (!isspace(ch))
                    return (-1);

            /*
             * Now make sure for cases 2 and 3 that the "extra"
             * bits that slopped past the last full byte were
             * zeros.  If we don't check them, they become a
             * subliminal channel.
             */
            if ((*decoded)[tarindex] != 0)
                return (-1);
        }
    } else {
        /*
         * We ended by seeing the end of the string.  Make sure we
         * have no partial bytes lying around.
         */
        if (state != 0) return (-1);
    }
    
    *len = tarindex;

    return 0;
}

static char hex[] = "0123456789abcdef";

/* ------------------------------------------------------------------------------ */

int hex2dec (unsigned char x)
{
    int l;
    
    if (x >= '0' && x <= '9') return x-'0';
    l = tolower (x);
    if (l >= 'a' && l <= 'f') return l-'a'+10;
    return 0;
}

/* ------------------------------------------------------------------------------ */

unsigned char dec2hex (int x)
{
    if (x >= 0 && x <= 9) return x+'0';
    if (x >= 10 && x <= 15) return x-10+'a';
    return '0';
}

/* converts binary string into its hexadecimal representation. returns
pointer to malloc()ed buffer */
char *bin2hex (unsigned char *bin, int len)
{
    char *hx;
    int  i;
    
    hx = malloc (len*2+1);
    for (i=0; i<len; i++)
    {
        hx[i*2]   = hex [bin[i] / 16];
        hx[i*2+1] = hex [bin[i] % 16];
    }
    hx[len*2] = '\0';
    
    return hx;
}

/* converts string of hexadecimal chars into string. assigns bin and len. 
length of source must be even. */
void hex2bin (char *hx, char **bin, int *len)
{
    int  i;

    *len = strlen (hx)/2;

    *bin = malloc (*len+1);
    for (i=0; i<(*len)*2; i+=2)
    {
        (*bin)[i/2] = hex2dec (hx[i]) * 16 + hex2dec (hx[i+1]);
    }
    bin[*len] = '\0';
}

static char unsafe[] = ";?:@&=+ \"#%<>()[]{}',";

/* returns pointer to malloc()ed buffer holding URL-safe representation of
 string `src', i.e. with all suspicious chars replaced by %XX constructs
 5 Oct: removed slash "/" from unsafe list */
char *hexify (char *src)
{
    char *b = malloc (strlen(src)*3+1), *p1 = src, *p2 = b;

    while (*p1)
    {
        if (*p1 < ' ' || *p1 > '~' || strchr (unsafe, *p1) != NULL)
        {
            snprintf1 (p2, 4, "%%%02X", *((unsigned char *)p1));
            p2 += 3;
        }
        else
        {
            *p2 = *p1;
            p2++;
        }
        p1++;
    }
    *p2 = '\0';
    return b;
}

/* returns pointer to malloc()ed buffer holding URL-safe representation of
 string `src', i.e. with all suspicious chars replaced by %XX constructs */
char *hexify2 (char *src, char *unsafe)
{
    char *b = malloc (strlen(src)*3+1), *p1 = src, *p2 = b;

    while (*p1)
    {
        if (*p1 < ' ' || *p1 == '%' || strchr (unsafe, *p1) != NULL)
        {
            snprintf1 (p2, 4, "%%%02X", *((unsigned char *)p1));
            p2 += 3;
        }
        else
        {
            *p2 = *p1;
            p2++;
        }
        p1++;
    }
    *p2 = '\0';
    return b;
}

/* converts URL-encoding (%20) representations into plain string. 
 the conversion is done in-place */
void dehexify (char *s)
{
    char *p;
    
    for (p = s; *p && *(p+1) && *(p+2); p++)
    {
        if (*p == '%' &&
            isxdigit ((unsigned)(*(p+1))) &&
            isxdigit ((unsigned)(*(p+2))))
        {
            *p = hex2dec (p[1]) * 16 + hex2dec (p[2]);
            str_delete (s, p+1);
            str_delete (s, p+1);
        }
    }
}

/* encodes integer n into buffer (buffer length must be at least 5 bytes),
 returns number of bytes used in the buffer */
int vby_encode (char *buf, u_int32_t n)
{
    int nb;

    nb = 0;
    while (n >= 128)
    {
        buf[nb++] = (n & 0x7f) | 0x80;
        n >>= 7;
    }
    buf[nb++] = n;
    return nb;
}

/* encodes 64 bit integer n into buffer (buffer length must be at
 least 10 bytes), returns number of bytes used in the buffer */
int vby_encode64 (char *buf, u_int64_t n)
{
    int nb;

    nb = 0;
    while (n >= 128)
    {
        buf[nb++] = (n & 0x7f) | 0x80;
        n >>= 7;
    }
    buf[nb++] = n;
    return nb;
}

/* decodes integer n from buffer (buffer length must be at least 5 bytes),
 returns number of bytes used from the buffer */
int vby_decode (char *buf, u_int32_t *n)
{
    int nb;
    u_int32_t count, get;

    *n = 0;
    nb = 0;
    count = 0;
    while (TRUE) {
        if (((get = buf[nb++]) & 0x80) == 0x80) {
            /* For each time we get a group of 7 bits, need to left-shift the 
               latest group by an additional 7 places, since the groups were 
               effectively stored in reverse order.*/
            *n |= ((get & 0x7f) << count);
            count += 7;
        } else if (nb > (sizeof(u_int32_t))) {
            if (nb == ((sizeof(u_int32_t) + 1))
              && (get < (1 << (sizeof(u_int32_t) + 1)))) {
                /* its a large, but valid number */
                *n |= get << count;
                return nb;
            } else {
                /* we've overflowed, return error */
                return 0;
            }
        } else {
            /* Now get the final 7 bits, which need to be left-shifted by a 
               factor of 7. */
            *n |= get << count;
            return nb;
        }
    }
    return 0;
}

/* decodes 64 bit integer n from buffer (buffer length must be at
 least 10 bytes), returns number of bytes used from the buffer */
int vby_decode64 (char *buf, u_int64_t *n)
{
    int nb;
    u_int32_t count;
    u_int64_t get;

    *n = 0;
    nb = 0;
    count = 0;
    while (TRUE) {
        if (((get = buf[nb++]) & 0x80) == 0x80) {
            /* For each time we get a group of 7 bits, need to left-shift the 
               latest group by an additional 7 places, since the groups were 
               effectively stored in reverse order.*/
            *n |= ((get & 0x7f) << count);
            count += 7;
        } else if (nb > (sizeof(u_int64_t))) {
            if (nb == ((sizeof(u_int64_t) + 1))
              && (get < (1 << (sizeof(u_int64_t) + 1)))) {
                /* its a large, but valid number */
                *n |= get << count;
                return nb;
            } else {
                /* we've overflowed, return error */
                return 0;
            }
        } else {
            /* Now get the final 7 bits, which need to be left-shifted by a 
               factor of 7. */
            *n |= get << count;
            return nb;
        }
    }
    return 0;
}

/* skips integer from buffer (buffer length must be at least 5 bytes),
 returns number of bytes skipped from the buffer */
int vby_skip (char *buf)
{
    unsigned int ret = 1;
    int nb = 0;

    while (TRUE) {
        if (!(buf[nb++] & 0x80)) {
            return ret;
        }
        ret++;
    }
}

unsigned int vby_fread (FILE* fp, u_int32_t *n) {
    unsigned int ret = 1;
    unsigned int count = 0; 
    unsigned char byte;

    *n = 0;
    while (fread(&byte, 1, 1, fp)) {
        if (byte & 0x80) {
            /* For each time we get a group of 7 bits, need to left-shift the 
               latest group by an additional 7 places, since the groups were 
               effectively stored in reverse order.*/
            *n |= ((byte & 0x7f) << count);
            ret++;
            count += 7;
        } else if (ret > (sizeof(*n))) {
            if (ret == (sizeof(*n) + 1) 
              && (byte < (1 << (sizeof(*n) + 1)))) {
                /* its a large, but valid number */
                *n |= byte << count;
                return ret;
            } else {
                /* we've overflowed */

                /* pretend we detected it as it happened */
                fseek(fp, - (ret - (sizeof(*n) + 1)), SEEK_CUR);
                /*errno = EOVERFLOW;*/
                return 0;
            }
        } else {
            *n |= byte << count;
            return ret;
        }
    }

    /* got a read error */
    return 0;
}

unsigned int vby_fwrite(FILE* fp, u_int32_t n) {
    unsigned int ret = 1;
    unsigned char byte;

	while (n >= 128) {
        /* write the bytes out least significant to most significant */
        byte = (n & 0x7f) | 0x80;
        if (!fwrite(&byte, 1, 1, fp)) {
            return 0;
        }
		n >>= 7;
		ret++;
	}

    byte = n;
    if (!fwrite(&byte, 1, 1, fp)) {
        return 0;
    }
	
    return ret;
}

unsigned int vby_fscan(FILE* fp) {
    unsigned int ret = 1;
    unsigned char byte;

    while (fread(&byte, 1, 1, fp)) {
        if (byte & 0x80) {
            /* For each time we get a group of 7 bits, need to left-shift the 
               latest group by an additional 7 places, since the groups were 
               effectively stored in reverse order.*/
            ret++;
        } else if (ret > sizeof(unsigned long int)) {
            if (ret == (sizeof(unsigned long int) + 1) 
              && (byte < (1 << (sizeof(unsigned long int) + 1)))) {
                /* its a large, but valid number */
                return ret;
            } else {
                /* we've overflowed */

                /* pretend we detected it as it happened */
                fseek(fp, - (ret - (sizeof(unsigned long int) + 1)), SEEK_CUR);
                /*errno = EOVERFLOW;*/
                return 0;
            }
        } else {
            return ret;
        }
    }

    /* got a read error */
    return 0;
}

unsigned int vby_len(u_int32_t n) {
    unsigned int ret = 1;
    while (n >= 128) {
        n >>= 7;
        ret++;
    }
    return ret;
}

int vby_encode_s (char *buf, char *s)
{
    int ln = strlen (s);
    int nb = vby_encode (buf, ln);
    memcpy (buf+nb, s, ln);
    return ln+nb;
}

int vby_decode_s (char *buf, char **s)
{
    u_int32_t ln;
    int nb = vby_decode (buf, &ln);

    *s = xmalloc (ln+1);
    memcpy (*s, buf+nb, ln);
    (*s)[ln] = '\0';
    return ln+nb;
}

