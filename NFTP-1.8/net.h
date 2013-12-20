#ifndef NET_H_INCLUDED
#define NET_H_INCLUDED

#include "types1.h"

#define RCURDIR        site.dir[site.cdir]
#define RCURFILENO     RCURDIR.current_file
#define RFIRSTITEM     RCURDIR.first_item
#define RFILE(n)       RCURDIR.files[n]
#define RCURFILE       RFILE(RCURFILENO)
#define RNFILES        RCURDIR.nfiles

#define MAX_SITE    8

#define is_firewalled  (status.use_proxy && \
    /*!((site.u.ip & 0x00FFFFFF) == (site.l_ip & 0x00FFFFFF))*/ \
    !(site.u.flags & URL_FLAG_FIREWALLED))
#define is_http_proxy  (is_firewalled && options.firewall_type == 5)

extern struct _site       site;
extern struct _cchistory  cchistory;
extern struct _firewall   firewall;

RC   SendToServer (int retry, int *ftpcode, char *resp, char *s, ...);
RC   SendToServer2 (int retry, int *ftpcode, char *resp, char *s);
int  Login (url_t *u);
RC   Logoff (void);
RC   SetTransferMode (void);
void CloseControlConnection (void);

#endif
