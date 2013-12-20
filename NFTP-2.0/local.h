#ifndef LOCAL_H_INCLUDED
#define LOCAL_H_INCLUDED

#include "types1.h"

#define LCURFILENO(pn)     local[pn].dir.current
#define LFIRSTITEM(pn)     local[pn].dir.first
#define LFILE(pn,n)        local[pn].dir.files[n]
#define LCURFILE(pn)       local[pn].dir.files[local[pn].dir.current]
#define LNFILES(pn)        local[pn].dir.nfiles

extern struct _local       local[2];
extern struct _local_cache lcache;

#endif
