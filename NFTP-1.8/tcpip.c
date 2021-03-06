#include <fly/fly.h>

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "nftp.h"
#include "network.h"
#include "auxprogs.h"

int SetBlockingMode (int sock, int mode)
{
#ifdef __MINGW32__
    unsigned long v;
#else
    int v;
#endif

#ifdef __MINGW32__
    if (mode)
        v = 0;
    else
        v = 1;
    return ioctlsocket (sock, FIONBIO, &v);
#else
    if (mode)
        v = 0;
    else
        v = O_NONBLOCK;
    return fcntl (sock, F_SETFL, v);
#endif
}

/*
 connect to `port' of `ip'. Watches for Esc in keyboard input stream;
 if pressed, returns ERR_CANCELLED. If socket can't be created, returns
 ERR_FATAL. If socket can't be connected during cc_timeout seconds,
 returns ERR_TRANSIENT. On success, returns socket connected to remote
 site (which is >= 0). ip and port are in network byte order
 */

int Connect2 (unsigned long ip, int port)
{
    int     sock, key, rc, rc1;
#ifdef __MINGW32__
    int     err, errlen;
#else
    int     rc2;
#endif
    struct  sockaddr_in  conn;
    struct  in_addr in;
    struct  timeval timeout;
    fd_set  wrfds;
    double  target;

    dmsg ("Entering Connect()\n");
retry:
    
    // create socket
    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fly_ask_ok (ASK_WARN, M("Cannot obtain socket, giving up"));
        return ERR_FATAL;     // shouldn't happen
    }

    // prepare connection data
    memset (&conn, 0, sizeof (conn));
    conn.sin_family = AF_INET;
    conn.sin_port = port;
    conn.sin_addr.s_addr = ip;

    // try to connect
    in.s_addr = ip;
    PutLineIntoResp2 (M("Connecting to %s: Esc - abort, Space - retry..."), inet_ntoa (in));
    SetBlockingMode (sock, FALSE);

    rc = connect (sock, (struct sockaddr *)&conn, sizeof (struct sockaddr_in));
    if (rc < 0)
    {
#ifndef __MINGW32__    
        if (errno == EINPROGRESS || errno == EAGAIN)
#endif        
        {
            target = clock1 () + (double)options.cc_timeout;
            do
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000;
                errno = 0;
                FD_ZERO (&wrfds);
                FD_SET (sock, &wrfds);
                rc1 = select (FD_SETSIZE, NULL, &wrfds, NULL, &timeout);
                dmsg ("select() in Connect() returns %d; errno = %d\n", rc1, errno);
                if (rc1 == 1)
                {
                    // see if we really got a connection
#ifdef __MINGW32__
                    errlen = sizeof (err);
                    getsockopt (sock, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);
                    dmsg ("getsockopt() returns %d\n", err);
                    if (err != 0)
                    {
                        socket_close (sock);
                        return ERR_FATAL;
                    }
                    break;
#else    
                    rc2 = connect (sock, (struct sockaddr *)&conn, sizeof (struct sockaddr_in));
                    dmsg ("second connect() returns %d; errno = %d\n", rc2, errno);
                    if (rc2 < 0 && errno == EISCONN) break; // good!
                    if (rc2 == 0) break; // good for OpenBSD?
                    if (rc2 < 0 && errno != EAGAIN)
                    {
                        socket_close (sock);
                        return ERR_FATAL;
                    }
#endif                
                }
                if (rc1 < 0 && errno == EINTR) continue; // suspended
                while ((key = getkey (0)) > 0)
                {
                    if (key == _Esc)
                    {
                        socket_close (sock);
                        dmsg ("Connect() returns cancelled\n");
                        return ERR_CANCELLED;
                    }
                    if (key == _Space)
                    {
                        socket_close (sock);
                        goto retry;
                    }
                }
            }
            while (clock1() < target);
        }
#ifndef __MINGW32__        
        else
        {
            socket_close (sock);
            dmsg ("Connect() returns FATAL 2\n");
            return ERR_FATAL;
        }
#endif        
    }

    //SetBlockingMode (sock, TRUE);

    PutLineIntoResp2 (M("Connected to port %u"), ntohs (port));
    dmsg ("Connect() returns OK\n");
    return sock;
}

/*
 accept connection from `port' of `ip'. Watches for Esc in keyboard input
 stream; if pressed, returns ERR_CANCELLED. If socket can't be created,
 returns ERR_FATAL. If socket can't be connected during cc_timeout seconds,
 returns ERR_TRANSIENT. On success, returns socket connected to remote
 site (which is >= 0).
 */

int Accept (int sock)
{
    struct sockaddr_in  conn, conn1;
    int     sl, sock1, rc1, key, rc;
    struct  timeval timeout;
    fd_set  rdfds;
    double  target;
    struct  in_addr in;

retry:
    
    memset (&conn, 0, sizeof (struct sockaddr_in));
    sl = sizeof (struct sockaddr_in);
    if (getsockname (sock, (struct sockaddr *)&conn, &sl) < 0)
        return ERR_TRANSIENT;

    PutLineIntoResp2 (M("Waiting for connection on port %d: Esc - abort, Space - retry..."), ntohs (conn.sin_port));
    SetBlockingMode (sock, FALSE);

    sl = sizeof (struct sockaddr_in);
    sock1 = accept (sock, (struct sockaddr *)&conn, &sl);
    dmsg ("sock1 = %d after first accept()\n", sock1);
    if (sock1 < 0)
    {
#ifndef __MINGW32__    
        if (errno == EINPROGRESS || errno == EAGAIN)
#endif
        {
            target = clock1 () + (double)options.cc_timeout;
            do
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000;
                FD_ZERO (&rdfds);
                FD_SET (sock, &rdfds);
                rc1 = select (FD_SETSIZE, &rdfds, NULL, NULL, &timeout);
                dmsg ("rc = %d, errno = %d after select() in Accept\n", rc1, errno);
                if (rc1 == 1) break;
                if (rc1 < 0 && errno == EINTR) continue; // suspended
                while ((key = getkey (0)) != 0)
                {
                    if (key == _Esc)
                    {
                        socket_close (sock);
                        return ERR_CANCELLED;
                    }
                    if (key == _Space)
                    {
                        socket_close (sock);
                        goto retry;
                    }
                }
            }
            while (clock1() < target);
            sl = sizeof (struct sockaddr_in);
            sock1 = accept (sock, (struct sockaddr *)&conn, &sl);
        }
#ifndef __MINGW32__        
        else
        {
            dmsg ("2. from Accept(): %d\n", ERR_FATAL);
            return ERR_FATAL;
        }
#endif        
    }
    
    //SetBlockingMode (sock1, TRUE);

    socket_close (sock);
    sl = sizeof (struct sockaddr_in);
    rc = getsockname (sock1, (struct sockaddr *)&conn1, &sl);
    if (rc == -1)
    {
        //fly_ask_ok (ASK_WARN, M("Network error (getsockname() failed)"));
        dmsg ("1. from Accept(): %d\n", ERR_FATAL);
        return ERR_TRANSIENT;
    }
    in.s_addr = conn1.sin_addr.s_addr;
    //if (conn.sin_addr.s_addr == 0) goto retry;
    PutLineIntoResp2 (M("Accepted connection from %s"), inet_ntoa (conn.sin_addr));
    dmsg ("normal return from Accept(): %d\n", sock1);
    return sock1;
}

/* creates socket, then binds it and puts into listening state.
 returns newly created socket, or error. Sets port (in host byte order)
 if no error. port is in network byte order
 */

int BindListen (int *port)
{
    struct sockaddr_in  conn;
    int    sock;
    int    rc, sl;
    
    // prepare to establish network connection
    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fly_ask_ok (ASK_WARN, M("Cannot obtain socket, giving up"));
        return ERR_FATAL;     // shouldn't happen
    }

    memset (&conn, 0, sizeof (conn));
    conn.sin_family = AF_INET;
    conn.sin_port = 0;
    conn.sin_addr.s_addr = INADDR_ANY;
    rc = bind (sock, (struct sockaddr *)&conn, sizeof (struct sockaddr_in));
    if (rc < 0)
    {
        fly_ask_ok (ASK_WARN, M("Network error (bind() failed)"));
        socket_close (sock);
        return ERR_FATAL;     // shouldn't happen
    }

    // get address of socket
    memset (&conn, 0, sizeof (struct sockaddr_in));
    sl = sizeof (struct sockaddr_in);
    rc = getsockname (sock, (struct sockaddr *)&conn, &sl);
    if (rc)
    {
        //fly_ask_ok (ASK_WARN, M("Network error (getsockname() failed)"));
        socket_close (sock);
        return ERR_TRANSIENT;     // shouldn't happen
    }
    *port = conn.sin_port;

    rc = listen (sock, 1);
    if (rc < 0)
    {
        fly_ask_ok (ASK_WARN, M("Network error (listen() failed)"));
        socket_close (sock);
        return ERR_FATAL;        // shouldn't happen
    }
    
    return sock;
}
