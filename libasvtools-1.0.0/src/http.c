#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static struct
{
    int  resp, nhdr, nhdr_a;
    char *response;
    char **hdrlines;
}
reply =
{
    507, 0, 0,
    NULL,
    NULL
};

int hdr_init (int code, char *errmsg)
{
    int i;

    if (reply.response != NULL) free (reply.response);
    for (i=0; i<reply.nhdr; i++)
    {
        free (reply.hdrlines[i]);
    }

    reply.resp = code;
    reply.response = strdup (errmsg);
    reply.nhdr = 0;

    if (reply.nhdr_a == 0)
    {
        reply.nhdr_a = 16;
        reply.hdrlines = malloc (sizeof(char *) * reply.nhdr_a);
        if (reply.hdrlines == NULL) return -1;
    }

    return 0;
}

int hdr_set_response (int code, char *format, ...)
{
    char     buffer[32768], *p;
    va_list  args;

    if (code < 100 || code > 999) return -1;

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);
    str_strip (buffer, "\r\n");

    p = strdup (buffer);
    if (p == NULL) return -1;

    reply.resp = code;
    reply.response = p;

    return 0;
}

int hdr_add (char *format, ...)
{
    char     buffer[32768], *p;
    va_list  args;

    va_start (args, format);
    vsnprintf1 (buffer, sizeof(buffer), format, args);
    va_end (args);
    /* trying to avoid strings with \r or \n at the end which
     will cause malformed headers */
    str_strip (buffer, "\r\n");

    p = strdup (buffer);
    if (p == NULL) return -1;

    return hdr_addraw (p);
}

int hdr_addraw (char *s)
{
    if (reply.nhdr == reply.nhdr_a)
    {
        reply.nhdr_a *= 2;
        reply.hdrlines = realloc (reply.hdrlines, sizeof(char *) * reply.nhdr_a);
        if (reply.hdrlines == NULL) return -1;
    }

    reply.hdrlines[reply.nhdr++] = s;

    return 0;
}

int hdr_send (FILE *fp)
{
    int i;

    fprintf (fp, "%3u %s\n", reply.resp,
             reply.response == NULL ? "" : reply.response);

    for (i=0; i<reply.nhdr; i++)
        fprintf (fp, "%s\n", reply.hdrlines[i]);
    fprintf (fp, "\n");

    return 0;
}

int hdr_write (int s)
{
    int i;

    XSend (s, "%3u ", reply.resp);

    Send (s, reply.response == NULL ? "" : reply.response);
    Send (s, "\n");

    for (i=0; i<reply.nhdr; i++)
    {
        Send (s, reply.hdrlines[i]);
        Send (s, "\n");
    }
    Send (s, "\n");

    return 0;
}

/* parses HTTP-style response in buffer 's'. 'headers' become filled with
 header information, n_headers will contain number of headers returned,
 'body' will point at the start of the actual response body. Original
 buffer 's' is not preserved, 'headers' itself is malloc()ed but
 (*headers)[i].{name|value} are not, they are pointers into 's'. returns
 responce code or negative value on error. First line of the response
 (with a code) is returned in (*headers)[0].value. */
int http_parse (char *s, int length, header_line_t **headers, int *n_headers, char **body)
{
    int  resp_code, nl, nl_a, i, chunk_size, nw, j, k, l, m;
    char **hdrlines, *p1, *entity_body, **L;
    unsigned char *p;

    resp_code = -1;

    /* verify sanity of input: it must be at least 5 bytes long and
     NUL-terminated at specified length */
    if (length < 5) return -4;
    /* if (s[length] != '\0') return -5; */

    /* check the status-line */
    if (!isdigit((unsigned char)(s[0])) ||
        !isdigit((unsigned char)(s[1])) ||
        !isdigit((unsigned char)(s[2])) ||
        s[3] != ' '
       )
    {
        /* new-style status line */
        p = (unsigned char *)strchr (s, ' ');
        if (p == NULL) return -1;
        p++;
        if (strlen ((char *)p) < 4) return -2;
        if (!isdigit((unsigned char)(p[0])) ||
            !isdigit((unsigned char)(p[1])) ||
            !isdigit((unsigned char)(p[2])) ||
            (p[3] != ' ' && p[3] != '\r')
           ) return -3;
        resp_code = (p[0]-'0')*100 + (p[1]-'0')*10 +(p[2]-'0');
    }
    else
    {
        /* old-style status line */
        resp_code = (s[0]-'0')*100 + (s[1]-'0')*10 +(s[2]-'0');
    }

    nl = 0;
    nl_a = 16;
    hdrlines = xmalloc (sizeof(char *) * nl_a);
    p = (unsigned char *)s;
    while (TRUE)
    {
        p1 = strchr ((char *)p, '\n');
        if (p1 == NULL)
        {
            free (hdrlines);
            return -2;
        }
        if (p1 != s && *(p1-1) == '\r') *(p1-1) = '\0';
        *p1++ = '\0';
        if (nl == nl_a)
        {
            nl_a *= 2;
            hdrlines = xrealloc (hdrlines, sizeof(char *) * nl_a);
        }
        hdrlines[nl++] = (char *)p;
        if (*p1 == '\r') p1++;
        /* headers have ended? */
        if (*p1 == '\n')
        {
            p1++;
            /* if (*p1 == '\r') p1++; */
            *body = p1;
            break;
        }
        p = (unsigned char *)p1;
    }

    if (nl == 0)
    {
        free (hdrlines);
        return -3;
    }

    /* parse header lines. preprocess 0-th line first */
    *headers = xmalloc (sizeof(header_line_t) * nl);
    (*headers)[0].name = "";
    (*headers)[0].value = hdrlines[0];
    for (i=1; i<nl; i++)
    {
        p = (unsigned char *)strchr (hdrlines[i], ':');
        if (p == NULL)
        {
            (*headers)[i].name = "";
            (*headers)[i].value = hdrlines[i];
        }
        else
        {
            *p = '\0';
            (*headers)[i].name = hdrlines[i];
            p++;
            while (*p && isspace (*p)) p++;
            (*headers)[i].value = (char *)p;
        }
    }
    free (hdrlines);

    /* RFC 2616
     length := 0
       read chunk-size, chunk-extension (if any) and CRLF
       while (chunk-size > 0) {
          read chunk-data and CRLF
          append chunk-data to entity-body
          length := length + chunk-size
          read chunk-size and CRLF
       }
       read entity-header
       while (entity-header not empty) {
          append entity-header to existing header fields
          read entity-header
       }
       Content-Length := length
       Remove "chunked" from Transfer-Encoding
       */

    /* Here's a quick-n-dirty patch for "chunked" transfer-encoding */
    for (i=0; i<nl; i++)
    {
        if (strcasecmp ((*headers)[i].name, "Transfer-Encoding") == 0 &&
            strstr ((*headers)[i].value, "chunked") != NULL)
        {
            /* start moving body characters to 'entity_body' which
             actually points into the same buffer. p1 is a scan pointer */
            p1 = *body;
            entity_body = p1;
            chunk_size = strtol (p1, NULL, 16);
            p1 = strstr (p1, "\r\n");
            if (p1 == NULL)
            {
                free (*headers);
                return -3;
            }
            p1 += 2;
            while (chunk_size > 0)
            {
                if (p1+chunk_size > s+length)
                {
                    /* corrupted document */
                    break;
                }
                memmove (entity_body, p1, chunk_size);
                entity_body += chunk_size;
                p1 += chunk_size;
                /* chunk is followed by \r\n */
                if (str_headcmp (p1, "\r\n") != 0)
                {
                    free (*headers);
                    return -3;
                }
                p1 += 2;
                if (p1 > s+length)
                {
                    /* corrupted document */
                    break;
                }
                /* size of the next chunk */
                chunk_size = strtol (p1, NULL, 16);
                p1 = strstr (p1, "\r\n");
                if (p1 == NULL)
                {
                    free (*headers);
                    return -3;
                }
                p1 += 2;
                if (p1 > s+length)
                {
                    /* corrupted document */
                    break;
                }
            }
            *entity_body = '\0';
            /* now we have to remove 'chunked' from Transfer-Encoding' */
            nw = str_words ((*headers)[i].value, &L, WSEP_SEMICOLON);
            if (nw > 0)
            {
                for (j=0, k=0; j<nw; j++)
                {
                    if (strcmp (L[j], "chunked") != 0) L[k++] = L[j];
                }
                if (k == 0)
                {
                    for (l=0, m=0; l<nl; l++)
                    {
                        if (l != i) (*headers)[m++] = (*headers)[l];
                    }
                    nl = m;
                    free (L);
                    goto done;
                }
                else
                {
                    p1 = str_mjoin (L, k, ';');
                    strcpy ((*headers)[i].value, p1);
                    free (p1);
                }
                free (L);
            }
        }
    }

done:
    *n_headers = nl;
    return resp_code;
}

/* read HTTP request from socket. 'headers' get filled with 'n_headers' items,
 'body' (not NULL if present) contains POST data. request is contained
 in headers[0].name. returns 0 on success, negative value on error. to
 free memory allocated by this call issue free() on every headers[i].name
 and headers itself; don't free headers[i].value, it is not malloc()ed but
 just a pointer into space pointed by headers[i].name */
int http_read_request (int sock, header_line_t **headers, int *n_headers,
                       char **body, int *n_body)
{
    int   i, n_headers_a;
    char  *m, *p;

    n_headers_a = 8;

    /* initialize returned fields */
    *n_headers = 0;
    *headers = xmalloc (sizeof (header_line_t) * n_headers_a);
    *n_body = 0;
    *body = NULL;

    /* read request */
    m = Recv (sock);
    /* warning1 ("got request %s\n", m); */
    if (m == NULL) goto stop_reading;
    (*headers)[*n_headers].name = m;
    (*headers)[*n_headers].value = NULL;
    (*n_headers)++;

    /* now read headers */
    while (TRUE)
    {
        m = Recv (sock);
        /* warning1 ("got header %s\n", m); */
        /* stop on error */
        if (m == NULL) goto stop_reading;

        /* HTTP request ends with an empty line */
        if (m[0] == '\0')
        {
            free (m);
            break;
        }

        p = strstr (m, ": ");
        if (p == NULL)
        {
            free (m);
            goto stop_reading;
        }
        *p = '\0'; p += 2;

        /* make sure *headers has enough space */
        if (*n_headers == n_headers_a)
        {
            n_headers_a *= 2;
            *headers = xrealloc (*headers, sizeof (header_line_t) * n_headers_a);
        }

        (*headers)[*n_headers].name = m;
        (*headers)[*n_headers].value = p;
        (*n_headers)++;

        free (m);
    }

    /* warning1 ("HTTP request has been read successfully\n"); */

    return 0;

stop_reading:
    for (i=0; i<*n_headers; i++)
    {
        if ((*headers)[i].name != NULL) free ((*headers)[i].name);
    }
    free (*headers);
    *headers = NULL;

    return -1;
}
