
#include <sys/types.h>

#ifndef __MINGW32__
        #include <netinet/in.h>
        #include <netdb.h>
        #include <sys/socket.h>
#endif

#if (!defined (__BEOS__) && !defined (__MINGW32__))
        #include <arpa/inet.h>
        //#include <sys/errno.h>
	#define socket_close close
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

#ifdef __EMX__
        #include <sys/select.h>
        #include <io.h>
        #define MAXHOSTNAMELEN 256
#endif

//#include "tcpip.h"
