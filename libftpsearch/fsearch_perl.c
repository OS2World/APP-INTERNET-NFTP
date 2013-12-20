#include "fsearch.h"
#include "fsearch_perl.h"

#include <asvtools.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern Results *RR;
extern int     *slots;

/* ====================================================================================== */

int FTPS_delete (int index)
{
    int  n, i;

    if (index < 0) return -1;
    if (slots[index] == 0) return -1;

    // compute new number of hits
    n = 0;
    for (i=0; i<RR[index].n; i++)
    {
        if (S_ISREG(RR[index].hits[i].rights))
        {
            if (i != n)
            {
                RR[index].hits[n] = RR[index].hits[i];
            }
            n++;
        }
        else
        {
            free (RR[index].hits[i].hostname);
            free (RR[index].hits[i].pathname);
            free (RR[index].hits[i].filename);
            if (RR[index].hits[i].userid != NULL)   free (RR[index].hits[i].userid);
            if (RR[index].hits[i].password != NULL) free (RR[index].hits[i].password);
        }
    }
    RR[index].n = n;

    return 0;
}

/* ====================================================================================== */

int FTPS_get_item (int index, int n, int *type, int *rights, int *size, int *timestamp)
{
    if (index < 0) return -1;
    if (slots[index] == 0) return -1;
    if (n >= RR[index].n) return -1;

    if (S_ISDIR(RR[index].hits[n].rights))      *type = T_DIR;
    else if (S_ISLNK(RR[index].hits[n].rights)) *type = T_LNK;
    else if (S_ISREG(RR[index].hits[n].rights)) *type = T_REG;
    else                                 *type = T_OTH;
    
    *rights    = RR[index].hits[n].rights & 0777;
    *size      = (int)(RR[index].hits[n].size);
    *timestamp = (int)(RR[index].hits[n].timestamp);

    return 0;
}

/* ====================================================================================== */

char *FTPS_get_hostname (int index, int n)
{
    if (index < 0) return NULL;
    if (slots[index] == 0) return NULL;
    if (n >= RR[index].n) return NULL;

    return RR[index].hits[n].hostname;
}

/* ====================================================================================== */

char *FTPS_get_pathname (int index, int n)
{
    if (index < 0) return NULL;
    if (slots[index] == 0) return NULL;
    if (n >= RR[index].n) return NULL;

    return RR[index].hits[n].pathname;
}

/* ====================================================================================== */

char *FTPS_get_filename (int index, int n)
{
    if (index < 0) return NULL;
    if (slots[index] == 0) return NULL;
    if (n >= RR[index].n) return NULL;

    return RR[index].hits[n].filename;
}

/* ====================================================================================== */

char *FTPS_get_login (int index, int n)
{
    if (index < 0) return NULL;
    if (slots[index] == 0) return NULL;
    if (n >= RR[index].n) return NULL;

    return RR[index].hits[n].userid;
}

/* ====================================================================================== */

char *FTPS_get_password (int index, int n)
{
    if (index < 0) return NULL;
    if (slots[index] == 0) return NULL;
    if (n >= RR[index].n) return NULL;

    return RR[index].hits[n].password;
}

