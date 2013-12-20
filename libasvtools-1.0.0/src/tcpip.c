#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef __MINGW32__
        #include <netinet/in.h>
        #include <netdb.h>
        #include <sys/socket.h>
        #ifndef __BEOS__
        #include <sys/select.h>
	#endif
#endif

#ifdef __BEOS__
        #define SO_KEEPALIVE 0
        #define socket_close closesocket
#endif

#ifdef __MINGW32__
        #include <winsock.h>
        #include <process.h>
        #define socket_close closesocket
        #define SIGPIPE 13
        #define getlogin() "win32user"
	#define EINPROGRESS     36
#endif

#if !defined(socket_close)
        #define socket_close close
#endif

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* this is a timeout limit for socket operations, in seconds.
 -1 means "no limit". */
static double timelimit = -1;

/* set timeout */
void Set_TCPIP_timeout (double dt)
{
    timelimit = dt;
}

/* connect to site/port */
int Connect (char *hostname, int port)
{
    struct  hostent *remote;
    unsigned long      addr;
    
    if (strspn (hostname, " .0123456789") == strlen (hostname))
    {
        addr = inet_addr (hostname);
        return ConnectIP (addr, port);
    }
    else
    {
        /* resolve hostname */
        remote = gethostbyname (hostname);
        if (remote == NULL)
        {
            /* warning1 ("cannot resolve %s\n", hostname); */
            return -1;
        }

        return ConnectIP (*((unsigned long *)(remote->h_addr)), port);
    }
}

/* connect to IPv4 address/port */
int ConnectIP (unsigned long IP, int port)
{
    struct  sockaddr_in conn;
    int     s, rc;
    int     rc1;
    struct  timeval timeout;
    fd_set  wrfds;
    double  target;
#if defined(__MINGW32__)
    unsigned long blk;
#endif    

    /* create socket */
    s = socket (AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        /* warning1 ("cannot create socket\n"); */
        return -2;
    }
    
    memset (&conn, 0, sizeof (conn));
    conn.sin_family = AF_INET;
    conn.sin_port = htons (port);
    conn.sin_addr.s_addr = IP;

    /* set nonblocking mode */
#if !defined(__MINGW32__)
    fcntl (s, F_SETFL, O_NONBLOCK);
#else
    blk = 1;
    ioctlsocket (s, FIONBIO, &blk);
#endif
    rc = connect (s, (struct sockaddr *)&conn, sizeof (struct sockaddr_in));

    if (rc < 0)
    {
        if (errno != EINPROGRESS && errno != EAGAIN)
        {
            Close (s);
            return -3;
        }

        target = clock1 () + timelimit;
        do
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            errno = 0;
            FD_ZERO (&wrfds);
            FD_SET (s, &wrfds);
            rc1 = select (FD_SETSIZE, NULL, &wrfds, NULL, &timeout);
            if (rc1 == 1)
            {
#if !defined(__MINGW32__)
                int rc2;
                /* see if we really got a connection */
                rc2 = connect (s, (struct sockaddr *)&conn, sizeof (struct sockaddr_in));
                if (rc2 < 0 && errno == EISCONN) goto success; /* good! */
                if (rc2 == 0) goto success; /* good for OpenBSD? */
                if (rc2 < 0 && errno != EAGAIN)
                {
                    Close (s);
                    return -3;
                }
#else
                goto success;
#endif
            }
            if (rc1 < 0 && errno == EINTR) continue; /* suspended */
        }
        while (clock1() < target);
        Close (s);
        return -4;
    }

success:
    /* reset nonblocking mode */
#if !defined(__MINGW32__)
    fcntl (s, F_SETFL, 0);
#else
    blk = 0;
    ioctlsocket (s, FIONBIO, &blk);
#endif

    return s;
}

/* send a string to socket. returns -1 on error, or number of chars sent */
int Send (int sock, char *buf)
{
    int n, len = strlen (buf);

    n = send (sock, buf, len, 0);
    if (n != len) return -1;

    return len;
}

/* format and send a string to socket */
int XSend (int sock, char *s, ...)
{
    va_list       args;
    char          buffer[16384];

    va_start (args, s);
    vsnprintf1 (buffer, sizeof(buffer), s, args);
    va_end (args);

    return Send (sock, buffer);
}

static char  *leftover = NULL;

/* read a string from socket. returns malloc()ed buffer with
 \n\r stripped, or NULL on error */
char *Recv (int sock)
{
    char         buf[1024], *p, *result;
    int          n;

reconsider:
    /* see if we already have string ready in the buffer */
    if (leftover != NULL)
    {
        p = strchr (leftover, '\n');
        if (p != NULL)
        {
            *p = '\0';
            result = strdup (leftover);
            str_strip (result, "\r\n");
            memmove (leftover, p+1, strlen(p+1)+1);
            return result;
        }
    }

    n = recv (sock, buf, sizeof(buf)-1, 0);
    /* check for error reading from socket. note that we discard something
     which could still be in the leftover buffer: every valid input line must
     be terminated with \n */
    if (n <= 0)
    {
        if (leftover != NULL) free (leftover);
        leftover = NULL;
        Close (sock);
        return NULL;
    }
    /* we got something */
    buf[n] = '\0';

    /* check if buffer contains NULLs. if yes we are getting some binary data,
     and this is BAD THING */
    if (n != strlen (buf))
    {
        if (leftover != NULL) free (leftover);
        leftover = NULL;
        Close (sock);
        return NULL;
    }

    /* if leftover is present, we concatenate it with recent input and then
     consider result. otherwise we copy the input to leftover and again
     proceed to buffer re-examination */
    if (leftover != NULL)
    {
        p = str_join (leftover, buf);
        free (leftover);
        leftover = p;
    }
    else
    {
        leftover = strdup (buf);
    }
    goto reconsider;
}

/* close a socket */
int Close (int sock)
{
    return socket_close (sock);
}

/* read binary data from socket until exhaustion.
 returns number of bytes read, or negative value on
 error. does not close socket. 'buf' is returned,
 malloc()ed (and NUL-terminated, but NUL byte
 is not included into the returned byte count).
 when 0 is returned, 'buf' is not allocated. */
int BRecv (int sock, char **buf)
{
    int   n, na, nb, bufsize, rc;
    char  *buffer, *p;
    struct  timeval timeout;
    fd_set  rdfds;

    bufsize = 8192;
    buffer  = xmalloc (bufsize);

    /* see if we already have string ready in the buffer */
    if (leftover != NULL)
    {
        na = bufsize + strlen (leftover);
        n  = strlen (leftover);
        p  = xmalloc (na);
        memcpy (p, leftover, n);
    }
    else
    {
        na = 8192;
        n  = 0;
        p  = xmalloc (na);
    }

    while (TRUE)
    {
        if (timelimit != -1.0)
        {
            timeout.tv_sec = timelimit;
            timeout.tv_usec = 0;
            FD_ZERO (&rdfds);
            FD_SET (sock, &rdfds);
            nb = -1;
            rc = select (FD_SETSIZE, &rdfds, NULL, NULL, &timeout);
            if (rc != 1)
            {
                break;
            }
        }

        nb = recv (sock, buffer, bufsize, 0);
        if (nb <= 0) break;
        if (n+nb > na-1)
        {
            na = max1 (na*2, n+nb+1);
            p = xrealloc (p, na);
        }
        memcpy (p+n, buffer, nb);
        n += nb;
    }
    free (buffer);

    if (n == 0)
    {
        free (p);
        if (nb < 0) return -1;
        return 0;
    }

    /* trim buffer allowing one extra byte for NUL */
    p = xrealloc (p, n+1);
    p[n] = '\0';
    *buf = p;

    return n;
}
