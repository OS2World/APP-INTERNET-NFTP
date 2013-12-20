#ifndef __DIRCACHE_H_INCLUDED__
#define __DIRCACHE_H_INCLUDED__

#include <asvtools.h>

#define FF_MARKED     0x0001 /* file is marked */
#define FF_DIR        0x0002 /* file is a directory */
#define FF_LINK       0x0004 /* file is a link */
#define FF_REGULAR    0x0008 /* regular file */
#define FF_COMPSIZE   0x0010 /* complete size is known for this directory */
#define FF_MDTM       0x0020 /* exact modification time is known */

typedef struct
{
    int     no;        /* original no.; used to restore original order after sorts */
    char    *name;     /* file name, null-terminated */
    int     extension; /* extension, displacement in [name] */
    int     rights;    /* Unix access permissions */
    int64_t size;      /* file size, in bytes */
    time_t  mtime;     /* modification time, UTC */
    char    *rawtext;  /* raw text representation (as sent by server) */
    int     flags;     /* various flags */
    char    *cached;   /* if viewed previously */
    char    *desc;     /* description */
    int     lrecl;     /* LRECL for OS/390 datasets */
}
dc_file_t;

typedef struct
{
    char *name; /* fully qualified directory name, malloc()ed */
    char *updir; /* name of the upper-level directory. when NULL
    it will be made automatically from 'name' by cutting at last slash */

    int       nf; /* number of files in the directory */
    dc_file_t *f; /* list of files, malloc()ed */
}
dc_directory_t;



#endif /* __DIRCACHE_H_INCLUDED__ */
