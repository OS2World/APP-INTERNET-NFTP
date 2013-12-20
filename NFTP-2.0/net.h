#ifndef NET_H_INCLUDED
#define NET_H_INCLUDED

#include "types1.h"

//#define RCURDIR        site[current_site].dir[site[current_site].cdir]
//#define RCURFILENO     RCURDIR.current_file
//#define RFIRSTITEM     RCURDIR.first_item
//#define RFILE(n)       RCURDIR.files[n]
//#define RCURFILE       RFILE(RCURFILENO)
//#define RNFILES        RCURDIR.nfiles

#define RN_CURDIR      site[nsite].dir[site[nsite].cdir]
#define RN_CURFILE     site[nsite].dir[site[nsite].cdir].files[site[nsite].dir[site[nsite].cdir].current]
#define RN_FIRST       site[nsite].dir[site[nsite].cdir].first
#define RN_CURRENT     site[nsite].dir[site[nsite].cdir].current
#define RN_NF          site[nsite].dir[site[nsite].cdir].nfiles
#define RN_FILE(i)     site[nsite].dir[site[nsite].cdir].files[i]

#define is_firewalled(n)  (status.use_proxy[n] && !(site[n].u.flags & URL_FLAG_FIREWALLED))
#define is_http_proxy(n)  (is_firewalled(n) && options.firewall_type == 5)

extern struct _site       site[MAX_SITE];
extern struct _firewall   firewall;

RC   SendToServer (int nsite, int retry, int *ftpcode, char *resp, char *s, ...);
RC   SendToServer2 (int nsite, int retry, int *ftpcode, char *resp, char *s);
int  Login (int nsite, url_t *u, int pnl);
RC   Logoff (int nsite);
RC   SetTransferMode (int nsite);
void CloseControlConnection (int nsite);
void cc_display (int once);

#endif
