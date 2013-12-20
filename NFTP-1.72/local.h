#ifndef LOCAL_H_INCLUDED
#define LOCAL_H_INCLUDED

#include "types1.h"

#define LCURFILENO     local.dir.current_file
#define LFIRSTITEM     local.dir.first_item
#define LFILE(n)       local.dir.files[n]
#define LCURFILE       LFILE(LCURFILENO)
#define LNFILES        local.dir.nfiles

extern struct _local local;
extern struct _local_cache lcache;

int l_chdir (char *newdir);

#endif
