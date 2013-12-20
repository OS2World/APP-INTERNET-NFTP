#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* returns 0 on success, 1 if file already exists and process is running
 and -1 on error */
int pidfile_create (char *pidfile, pid_t pid)
{
    int fh, rc;
    FILE *fp;

    while (TRUE)
    {
        /* we try to create pidfile but fail if it already exists */
        fh = open (pidfile, O_RDWR|O_CREAT|O_EXCL, 0666);
        if (fh >= 0) break;

        if (errno != EEXIST) return -1;
        /* if process is already active return 1 */
        rc = pidfile_check (pidfile);
        switch (rc)
        {
        case  1: return 1;
        case -1: return -1;
        }
    }

    fp = fdopen (fh, "w");
    if (fp == NULL)
    {
        close (fh);
        return -1;
    }
    fprintf (fp, "%d\n", (int)pid);
    if (fclose (fp) < 0) return -1;

    return 0;
}

/* returns 0 on success, 1 if file does not exists and -1 on error */
int pidfile_remove (char *pidfile)
{
    int rc;

    rc = unlink (pidfile);
    if (rc < 0)
    {
        if (errno == ENOENT) return 1;
        return -1;
    }

    return 0;
}

/* returns 0 if pidfile does not exist; 1 if pidfile exists and process is
 running; 2 if pidfile exists and process is not running (pidfile gets
 deleted in this case), -1 on error */
int pidfile_check (char *pidfile)
{
    int fh, nf, pid;
    FILE *fp;

    fh = open (pidfile, O_RDONLY);
    if (fh < 0)
    {
        if (errno == ENOENT) return 0;
        return -1;
    }

    fp = fdopen (fh, "r");
    if (fp == NULL)
    {
        close (fh);
        return -1;
    }
    nf = fscanf (fp, "%d\n", &pid);
    if (fclose (fp) < 0) return -1;

    /* we consider that process is not running if: pidfile is empty,
     or contains invalid data, or pid is not in the process table */
    if (nf < 1 || pid == 0
#if HAVE_GETPGID
        || getpgid(pid) < 0
#endif
       )
    {
        if (unlink (pidfile) < 0) return -1;
        return 2;
    }

    return 1;
}
