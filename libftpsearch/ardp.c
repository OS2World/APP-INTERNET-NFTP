#include <asvtools.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#include "fsearch.h"
#include "network.h"

#define ARDP_MAX_HDR_LENGTH 13
#define PROSPERO_TIMEOUT    0.01

static struct
{
    unsigned             conn_id;
    struct sockaddr_in   server;
}
conn = {0};

static int in_sequence = 0;

struct ardpdata
{
    int version, hdrlen, connid, pktno,
    totalpkt, received_through, pause;
    
    int flag_address_information_follows, flag_priority_follows,
    flag_protocol_id_follows, flag_control_only, flag_need_ack, option;
};

enum {RECV_SIZE = 8192};

static double         rec_confirm = 0.;
static int            n_expected = 0, n_received = 0;
static unsigned char  *recv_buffer = NULL;

void ardp_parse_header (unsigned char *hdr, int len, struct ardpdata *a);
void ardp_print_packet (unsigned char *hdr, int len);
int  ardp_ping (int sock);

/* ----------------------------------------------------------------------------
 returns whether further input is expected or not */

int ardp_sequence (void)
{
    return in_sequence;
}

/* ----------------------------------------------------------------------------
 parses ARDP header */

void ardp_parse_header (unsigned char *hdr, int len, struct ardpdata *a)
{
    if (len >= 1)
    {
        a->version = (hdr[0] & 0xC0) >> 6;
        a->hdrlen = (hdr[0] & 0x3F);
    }
    else
    {
        a->version = 0;
        a->hdrlen = 0;
    }

    // check for broken packet
    if (a->hdrlen > len) a->hdrlen = len;

    if (a->hdrlen >= 3)
    {
        //a->connid = word2uint (hdr+1);
        a->connid = 256*hdr[1] + hdr[2];
    }
    else
    {
        a->connid = 0;
    }

    if (a->hdrlen >= 5)
    {
        //a->pktno = word2uint (hdr+3);
        a->pktno = 256*hdr[3] + hdr[4];
    }
    else
    {
        a->pktno = 1;
    }

    if (a->hdrlen >= 7)
    {
        //a->totalpkt = word2uint (hdr+5);
        a->totalpkt = 256*hdr[5] + hdr[6];
    }
    else
    {
        a->totalpkt = 0;
    }

    if (a->hdrlen >= 9)
    {
        //a->received_through = word2uint (hdr+7);
        a->received_through = 256*hdr[7] + hdr[8];
    }
    else
    {
        a->received_through = 0;
    }

    if (a->hdrlen >= 11)
    {
        //a->pause = word2uint (hdr+9);
        a->pause = 256*hdr[9] + hdr[10];
    }
    else
    {
        a->pause = 0;
    }

    if (a->hdrlen >= 12)
    {
        a->flag_address_information_follows = hdr[11] & 0x01 ? 1 : 0;
        a->flag_priority_follows = hdr[11] & 0x02 ? 1 : 0;
        a->flag_protocol_id_follows = hdr[11] & 0x04 ? 1 : 0;
        a->flag_control_only = hdr[11] & 0x40 ? 1 : 0;
        a->flag_need_ack = hdr[11] & 0x80 ? 1 : 0;
    }
    else
    {
        a->flag_address_information_follows = 0;
        a->flag_priority_follows = 0;
        a->flag_protocol_id_follows = 0;
        a->flag_control_only = 0;
        a->flag_need_ack = 0;
    }

    if (a->hdrlen >= 13)
    {
        a->option = hdr[12];
    }
    else
    {
        a->option = 0;
    }
}

/* ----------------------------------------------------------------------------
 sets up `connection' to specified site, i.e. establishes information about where
 to send/receive packets. returns socket descriptor if OK, -1 if error */

int ardp_open (char *hostname, int port)
{
    struct hostent *he;
    int            sock;

    //debug_tools ("ardp_open\n");
    // create datagram socket
    sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    // find the IP address of target server
    he = gethostbyname (hostname);
    if (he == NULL)
    {
        socket_close (sock);
        return -1;
    }
    conn.server.sin_family = AF_INET;
    conn.server.sin_port   = htons (port);
    conn.server.sin_addr.s_addr = *((unsigned long *)(he->h_addr));

    recv_buffer = malloc (RECV_SIZE);
    if (recv_buffer == NULL)
    {
        socket_close (sock);
        return -1;
    }

    // set connection id
    if (conn.conn_id != 0) conn.conn_id++;
    else                   conn.conn_id = getpid ();

    // set our internal variables
    in_sequence = 1;
    n_expected = 0;
    n_received = 0; // nothing has been received yet
    rec_confirm = 0.;
    
    return sock;
}

/* ----------------------------------------------------------------------------
 destroys current `connection', i.e. free all resources associated with it */

void ardp_close (int sock)
{
    if (sock != -1) socket_close (sock);
    
    if (recv_buffer != NULL)
    {
        free (recv_buffer);
        recv_buffer = NULL;
    }
}

/* ----------------------------------------------------------------------------
 sends data (NULL-terminated string) to server. returns -1 if error */

int ardp_send (int sock, char *s)
{
    unsigned char   *data;
    int    ls, rc;

    ls = strlen (s);
    data = malloc (ls + 11 + ARDP_MAX_HDR_LENGTH);
        
    // version = 0, header_length = 7, connection id = -11, packet # = 1, total # of packets = 1
    // received_through = 0, pause = 0, addr_info = 0, priority = 0, prot_id = 0, cntrl = 0
    // need_ack = 0, option = 0
    
    // build standard `data' ARDP client-to-server packet
    memset (data, 0, 11);

    data[0] = 11; // header length
    data[1] = conn.conn_id/256;   // connection id; MSB
    data[2] = conn.conn_id%256;   // connection id; LSB
    data[3] = 0;  // packet no; MSB
    data[4] = 1;  // packet no; LSB
    data[5] = 0;  // total # of packets; MSB
    data[6] = 1;  // total # of packets; LSB
    data[7] = 0;  // received through; MSB
    data[8] = 0;  // received through; LSB
    data[9] = 0;  // wait; MSB
    data[10] = 2;  // wait; LSB

    // append data to header
    strcpy (data+11, s);
    
    // send
    rc = sendto (sock, data, ls+11, 0, (struct sockaddr *)&conn.server, sizeof (struct sockaddr));
    free (data);
    if (rc != ls+11) return -1;
    
    return rc;
}

/* ----------------------------------------------------------------------------
 confirms receiving packets, returns -1 if error */

int ardp_ping (int sock)
{
    unsigned char   data[13];
    int    rc;

    //if (i <= 0) return 0; // don't confirm 0 packets received :-)
    if (n_received == 0) return 0;
    
    // build standard `data' ARDP client-to-server packet
    memset (data, 0, 12);
    
    // version = 0, header_length = 12, connection id = 5
    // packet # = 0, total # of packets = 1
    // received_through = 26, pause = 0, addr_info = 0, priority = 0, prot_id = 0, cntrl = 1
    // need_ack = 0, option = 0

    data[0] = 12; // header length
    data[1] = conn.conn_id/256;   // connection id; MSB
    data[2] = conn.conn_id%256;   // connection id; LSB
    data[3] = 0;  // packet no; MSB
    data[4] = 0;  // packet no; LSB
    data[5] = 0;  // total # of packets; MSB
    data[6] = 1;  // total # of packets; LSB
    data[7] = n_received/256; // received; MSB
    data[8] = n_received%256; // received; LSB
    data[9] = 0;   // pause; MSB
    data[10] = 0;  // pause; LSB
    data[11] = 0x40; // control-only (no data)

    // send
    rc = sendto (sock, data, 12, 0, (struct sockaddr *)&conn.server, sizeof (struct sockaddr));
    if (rc != 12) return -1;
    
    return rc;
}

/* ----------------------------------------------------------------------------
 cancels request; returns -1 if error */

int ardp_cancel (int sock, int id)
{
    unsigned char   data[14];
    int    rc;

    //debug_tools ("ardp_cancel\n");
    // build standard `data' ARDP client-to-server packet
    memset (data, 0, 13);
    
    // version = 0, header_length = 13, connection id = 690
    // packet # = 0, total # of packets = 0
    // received_through = 0, pause = 0, addr_info = 0, priority = 0, prot_id = 0, cntrl = 0
    // need_ack = 0, option = 1
    
    data[0] = 13; // header length
    if (id < 0)
    {
        data[1] = conn.conn_id/256;   // connection id; MSB
        data[2] = conn.conn_id%256;   // connection id; LSB
    }
    else
    {
        data[1] = id/256;   // connection id; MSB
        data[2] = id%256;   // connection id; LSB
    }
    data[3] = 0;  // packet no; MSB
    data[4] = 0;  // packet no; LSB
    data[5] = 0;  // total # of packets; MSB
    data[6] = 0;  // total # of packets; LSB
    data[7] = 0; // received; MSB
    data[8] = 0; // received; LSB
    data[9] = 0;   // pause; MSB
    data[10] = 0;  // pause; LSB
    data[11] = 0; // flags
    data[12] = 1; // option = cancel

    // send
    rc = sendto (sock, data, 13, 0, (struct sockaddr *)&conn.server, sizeof (struct sockaddr));
    if (rc != 13) return -1;
    
    return rc;
    
}

/* ----------------------------------------------------------------------------
 receives data from server. returns length of malloc()ed buffer (s), or -1 if error.
 0 indicates that pending data were discarded and no useful input is available */

int ardp_receive (int sock, char **s)
{
    int                  n, rec, sa_len;
    struct sockaddr_in   source;
    struct ardpdata      a;

    *s = NULL;
    
    // receive data
    //debug_tools ("ardp_receive\n");
    sa_len = sizeof (struct sockaddr_in);
    rec = recvfrom (sock, recv_buffer, RECV_SIZE, 0, (struct sockaddr *) &source, &sa_len);
    //debug_tools ("recvfrom() returned %d\n", rec);

    // source address valid?
    if (sa_len != sizeof (struct sockaddr_in))
    {
        //debug_tools ("wrong length of source address (%d)\n", sa_len);
        return 0;
    }

    // source address matches expected?
    if (source.sin_family != conn.server.sin_family ||
        source.sin_port != conn.server.sin_port /*||
        source.sin_addr.s_addr != conn.server.sin_addr.s_addr*/)
    {
        //debug_tools ("wrong source address:\n"
        //             "received: family = %u, port = %u, ip = %lu;\n"
        //             "expected: family = %u, port = %u, ip = %lu\n",
        //             source.sin_family, source.sin_port,
        //             source.sin_addr.s_addr,
        //             conn.server.sin_family, conn.server.sin_port,
        //             conn.server.sin_addr.s_addr);
        return 0;
    }

    if (rec <= 0)
    {
        //debug_tools ("recvfrom() returned %d\n", rec);
        return 0;
    }
    
    // parse header
    ardp_parse_header (recv_buffer, rec, &a);
    in_sequence = 1;
    
    // check whether we should send a confirmation
    if (a.flag_control_only)
    {
        ardp_ping (sock);
        return 0;
    }
    
    // check option
    if (a.totalpkt > n_expected) n_expected = a.totalpkt;
    switch (a.option)
    {
    case 0:
        if (a.pktno == n_received + 1)
        {
            *s = recv_buffer+a.hdrlen;
            n_received++;
            n = rec-a.hdrlen;
            (*s)[n] = '\0';
            if (n_received == n_expected && n_expected != 0)
            {
                ardp_ping (sock);
                in_sequence = 0;
            }
            return n;
        }
        return 0;

    case 1:
        return -1;

    default:
        return 0;
    }
}

/* ----------------------------------------------------------------------------
 waits for data in the socket. also send confirmations when necessary. returns
 1 if data is available for reading, 0 if timeout, -1 if error */

int ardp_select (int sock, double t)
{
    struct timeval  tv;
    double          start, end, now;
    fd_set          rdfs;
    int             rc;

    //debug_tools ("ardp_select\n");
    start = clock1 ();
    end = start + t;

    if (rec_confirm == 0.) rec_confirm = start;
    do
    {
        FD_ZERO (&rdfs);
        FD_SET  (sock, &rdfs);
        tv.tv_sec  = 0;
        tv.tv_usec = 300000;
        rc = select (sock+1, &rdfs, NULL, NULL, &tv);
        //debug_tools ("ardp_select: select returns %d\n", rc);
        if (rc < 0) return -1;
        if (rc > 0) return 1;
        //fprintf (stderr, "timeout in ardp_select!\n");
        now = clock1 ();
        if (rec_confirm != 0. && now - rec_confirm > 2.0)
        {
            ardp_ping (sock);
            rec_confirm = now;
        }
    }
    while (now < end);
    
    //debug_tools ("ardp_select: timeout exit\n");
    return 0;
}
