/*
 Support for transferring/storing bfs (BeOS filesystem) attributes
 -----------------------------------------------------------------
 Written by Sergey Ayukov <asv@ayukov.com>
 This code is donated into the public domain, i.e. it can be freely
 used without any restrictions in any kind of software (including
 but not limited to open-source and commercial software). You can
 give the credit to the author but are not required to.

 Abbreviations used:
 AT -- attribute transfer
 AF -- attribute file, i.e. file containing attributes and their
 values; such file must have extension "-BFS-attributes-" by
 convention.

 How it works
 ------------
               Client: Non-BeOS     BeOS
 Server
 Non-BeOS                 1)          2)
 BeOS                     3)          4)

 There are four options in the table above.
 
 1) transfer between non-BeOS server and client. In this case
 AT hardly makes much sense and probably should be avoided.
 However, client must have an option (probably turned off by
 default) to guess whether server support AT or not.
 
 2) BeOS client works with non-BeOS server. This means client
 has queried server about AT extensions and got negative
 answer. The decision is whether to use AF is on the user
 (i.e. it is a configuration option); reasonable default
 is not to store attributes on such server. Of course,
 client could have special user interface option to transfer
 attributes.
 
 3) Non-BeOS client talks to BeOS server. Most likely, it does
 not need the attributes, and behaviour is similar to case 2
 except that if user did not configure his/her client to do the
 AT, client does not need to query server about AT capabilities.
 
 4) Normal situation when BeOS client wants to do AT with BeOS
 server.

 How it is implemented in NFTP
 -----------------------------
 There are two options in the nftp.ini concerning BFS attributes.
 
 ; whether to query server about BeOS filesystem attribute transfer
 ; support. by default, this option is "yes" in BeOS version and "no"
 ; in other NFTP versions
 ;
 ;query-bfs-attributes-support=

 ; whether to use files with ".-BFS-attributes-" extensions to store
 ; BeOS attributes when local filesystem or remote server do not support
 ; attributes natively. default: no
 ;
 ;use-bfs-files=no

 When 'query-bfs-attributes-support' is false, NFTP won't try to detect
 whether remote server supports attributes or not. If 'use-bfs-files'
 is also false, NFTP won't do anything concerning attributes. If
 'use-bfs-files' is true in this case (rather weird situation), NFTP
 will always transfer attributes via AFs.

 When 'query-bfs-attributes-support' is true, NFTP will send SITE HELP
 command as outlined in http://www.desertnights.com/attribute.shtml
 to find out whether server is AT-capable. If it is, NFTP will use
 LATT/RATT/SATT commands to retrieve/store attributes. If server does
 not support AT natively NFTP will again look at 'use-bfs-files' to
 determine whether to transfer attributes.

 In both cases BeOS version of NFTP will use native BeOS calls to
 fetch/attach attributes. Non-BeOS versions will use AFs.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <asvtools.h>

#ifdef __BEOS__
#include <be/KernelKit.h>
#endif

#include "befs.h"

enum {ATTR_LIMIT = 1024*1024};
static char *SIGNATURE   = "bfs attribute file version 1";
char *ATTR_SUFFIX =  ".-BFS-attributes-";

static int is_plaintext (char *buffer, int length);
static char *convert (char *buffer, char format, int *len);

/* ----------------------------------------------------------------------- */

#ifdef TEST
// first argument is the option name:
// l file-with-attrs               list
// d attrfile                      dump
// f file-with-attrs attrfile      fetch into file
// a attrfile file-with-attrs      attach attrs from file to file
int main (int argc, char *argv[])
{
    befs_attr    *attrs;
    int          nat, i;
    
    switch (argv[1][0])
    {
    case 'l':
        if (argc != 3) return -1;
        printf ("list for %s\n", argv[2]);
        nat = bfs_fetch (argv[2], &attrs);
        if (nat < 0) return 0;
        for (i=0; i<nat; i++)
        {
            printf ("%02d %4d bytes %s=%s\n", i, attrs[i].length, attrs[i].name,
                    attrs[i].value);
        }
        break;
        
    case 'd':
        if (argc != 3) return -1;
        printf ("dump from %s\n", argv[2]);
        nat = read_attrfile (argv[2], &attrs);
        if (nat < 0) return 0;
        for (i=0; i<nat; i++)
        {
            printf ("%02d %4d bytes %s=%s\n", i, attrs[i].length, attrs[i].name,
                    attrs[i].value);
        }
        break;
        
    case 'f':
        if (argc != 4) return -1;
        printf ("fetch from %s to %s\n", argv[2], argv[3]);
        nat = bfs_fetch (argv[2], &attrs);
        if (nat < 0) return 0;
        write_attrfile (argv[3], attrs, nat);
        break;
        
    case 'a':
        if (argc != 4) return -1;
        printf ("attach from %s to %s\n", argv[2], argv[3]);
        nat = read_attrfile (argv[2], &attrs);
        if (nat < 0) return 0;
        bfs_attach (argv[3], attrs, nat);
        break;
    }

    return 0;
}
#endif
/* ----------------------------------------------------------------------- */

#ifdef __BEOS__
int bfs_fetch (char *filename, befs_attr **attrs)
{
    DIR        *d;
    dirent_t   *ent;
    attr_info  info;
    int        fd, nalloc, rc, nattrs;
    char       *buffer;

    nattrs = 0;
    
    // open file to fetch its attributes
    fd = open (filename, O_RDONLY);
    if (fd < 0) return -1;

    // open attribute directory for specified file
    d = fs_fopen_attr_dir (fd);
    if (d == NULL)
    {
        close (fd);
        return -1;
    }

    // fetch attributes one-by-one
    nalloc = 16;
    *attrs = xmalloc (sizeof (befs_attr) * nalloc);
    while ((ent = fs_read_attr_dir(d)) != NULL)
    {
        rc = fs_stat_attr (fd, ent->d_name, &info);
        if (rc < 0) continue;
        buffer = xmalloc((size_t) info.size);
        rc = fs_read_attr(fd, ent->d_name, info.type, 0, buffer, info.size);
        if (rc < 0) continue;
        (*attrs)[nattrs].type   = info.type;
        (*attrs)[nattrs].name   = strdup (ent->d_name);
        (*attrs)[nattrs].value  = xmalloc (info.size);
        memcpy ((*attrs)[nattrs].value, buffer, info.size);
        (*attrs)[nattrs].length = info.size;
        free (buffer);
        nattrs++;
        if (nattrs == nalloc)
        {
            nalloc += 16;
            *attrs = realloc (*attrs, sizeof (befs_attr) * nalloc);
        }
    }
    
    close(fd);
    fs_close_attr_dir(d);
    if (nattrs == 0)
        free (*attrs);
    else
        *attrs = realloc (*attrs, sizeof (befs_attr) * nattrs);
    
    return nattrs;
}

/* ----------------------------------------------------------------------- */

int bfs_attach (char *filename, befs_attr *attrs, int nattrs)
{
    int   fd, i, rc;

    // open file to fetch its attributes
    fd = open (filename, O_WRONLY);
    debug_tools ("opening %s; got %d\n", filename, fd);
    if (fd < 0) return -1;

    // q: should we remove all present attributes at first??

    // attach attributes one-by-one
    for (i=0; i<nattrs; i++)
    {
        rc = fs_write_attr (fd, attrs[i].name, attrs[i].type,
                            0, attrs[i].value, attrs[i].length);
        debug_tools ("rc = %d on fs_write_attr\n", rc);
        if (rc < 0)
        {
            close (fd);
            return -1;
        }
    }
    close (fd);

    return 0;
}
#endif

/* ----------------------------------------------------------------------- */

#ifndef __BEOS__
int bfs_fetch (char *filename, befs_attr **attrs)
{
    char *attr_name;
    int  nat;
    
    attr_name = str_join (filename, ATTR_SUFFIX);
    nat = read_attrfile (attr_name, attrs);
    free (attr_name);
    if (nat < 0) return 0; // error reading? report no attributes
    
    return nat;
}

/* ----------------------------------------------------------------------- */

int bfs_attach (char *filename, befs_attr *attrs, int nattrs)
{
    char *attr_name;
    int  rc;
    
    attr_name = str_join (filename, ATTR_SUFFIX);
    rc = write_attrfile (attr_name, attrs, nattrs);
    free (attr_name);

    return rc;
}
#endif
        
/* ----------------------------------------------------------------------- */

static int is_plaintext (char *buffer, int length)
{
    int i;
    
    for (i=0; i<length; i++)
    {
        if ( 
             ((unsigned char)(buffer[i]) < ' ' || buffer[i] == '=' ) &&
             ! (i==length-1 && buffer[i] == 0)
           ) return FALSE;
    }
    
    return TRUE;
}

/* ----------------------------------------------------------------------- */

/*
bfs attribute file version 1
attribute-type,attribute-length,qq,name-as-plainstring=value-as-plainstring
attribute-type,attribute-length,qb,name-as-plainstring=value-as-base64string
attribute-type,attribute-length,bq,name-as-base64string=value-as-plainstring
attribute-type,attribute-length,bb,name-as-base64string=value-as-base64string
*/

int write_attrfile (char *filename, befs_attr *attrs, int nattrs)
{
    int     i;
    char    ns, vs, *nr, *vr;
    FILE    *fp;
    
    fp = fopen (filename, "w");
    if (fp == NULL) return -1;
    
    fprintf (fp, SIGNATURE);
    fprintf (fp, "\n");
    for (i=0; i<nattrs; i++)
    {
        // name of the attribute
        if (is_plaintext (attrs[i].name, strlen(attrs[i].name)))
        {
            ns = 'q';
            nr = attrs[i].name;
        }
        else
        {
            ns = 'b';
            nr = base64_encode ((unsigned char *)attrs[i].name, strlen(attrs[i].name));
        }
        // value of the attribute
        if (is_plaintext (attrs[i].value, attrs[i].length))
        {
            vs = 'q';
            vr = attrs[i].value;
        }
        else
        {
            vs = 'b';
            vr = base64_encode ((unsigned char *)attrs[i].value, attrs[i].length);
        }
        // actual output
        fprintf (fp, "%d,%d,%c%c,%s=%s\n", attrs[i].type, attrs[i].length, ns, vs, nr, vr);
        // free allocated arrays
        if (ns == 'b') free (nr);
        if (vs == 'b') free (vr);
    }
    fclose (fp);

    return 0;
}

/* ----------------------------------------------------------------------- */

int read_attrfile (char *filename, befs_attr **attrs)
{
    struct stat  st;
    int        nalloc, rc, len, nattrs;
    char       *buffer = NULL, *p1, *p2, *p3, *p4;
    FILE       *fp = NULL;

    // find out the size of the file
    rc = stat (filename, &st);
    if (rc) return -1;

    if (st.st_size > ATTR_LIMIT) return -1; // reject ridiculously large files

    buffer = malloc (st.st_size);
    if (buffer == NULL) return -1;
    
    nattrs = 0;
    
    // open file to fetch its attributes
    fp = fopen (filename, "r");
    if (fp == NULL) goto error_reading;

    // get the signature string
    if (fgets (buffer, st.st_size, fp) == NULL) goto error_reading;
    str_strip (buffer, "\r\n");
    if (strcmp (buffer, SIGNATURE)) goto error_reading;

    // fetch attributes one-by-one
    nalloc = 16;
    *attrs = malloc (sizeof (befs_attr) * nalloc);
    while (fgets (buffer, st.st_size, fp) != NULL)
    {
        str_strip (buffer, "\r\n");
        //attribute-type,attribute-length,qq,name-as-plainstring=value-as-plainstring
        // locate delimiters (three commas and equal sign)
        p1 = strchr (buffer, ',');
        if (p1 == NULL) continue;
        *p1++ = '\0';
        p2 = strchr (p1, ',');
        if (p2 == NULL) continue;
        *p2++ = '\0';
        p3 = strchr (p2, ',');
        if (p3 == NULL) continue;
        *p3++ = '\0';
        p4 = strchr (p3, '=');
        if (p4 == NULL) continue;
        *p4++ = '\0';
        // check the length of flags (should be 2)
        if (strlen (p2) != 2) continue;
        // assign values
        (*attrs)[nattrs].type  = atol (buffer);
        (*attrs)[nattrs].name  = convert (p3, p2[0], &len);
        if ((*attrs)[nattrs].name == NULL) continue;
        (*attrs)[nattrs].value = convert (p4, p2[1], &len);
        if ((*attrs)[nattrs].value == NULL)
        {
            free ((*attrs)[nattrs].name);
            continue;
        }
        (*attrs)[nattrs].length = atol (p1);
        //if (len != (*attrs)[nattrs].length)
        //{
        //    free ((*attrs)[nattrs].name);
        //    free ((*attrs)[nattrs].value);
        //    continue;
        //}
        nattrs++;
        if (nattrs == nalloc)
        {
            nalloc += 16;
            *attrs = realloc (*attrs, sizeof (befs_attr) * nalloc);
        }
    }
    
    if (nattrs == 0)
        free (*attrs);
    else
        *attrs = realloc (*attrs, sizeof (befs_attr) * nattrs);
    
    fclose (fp);
    free (buffer);
    
    return nattrs;

error_reading:
    if (fp != NULL) fclose (fp);
    if (buffer != NULL) free (buffer);
    return -1;
}

/* ----------------------------------------------------------------------- */

static char *convert (char *buffer, char format, int *len)
{
    char  *p;
    int   rc;
    
    switch (format)
    {
    case 'q':
        p = strdup (buffer);
        *len = strlen (p);
        return p;

    case 'b':
        rc = base64_decode ((unsigned char *)buffer, &p, len);
        if (rc) return NULL;
        return p;
        
    case 'x':
        hex2bin (buffer, &p, len);
        return p;
    }
    
    return NULL;
}

/* ----------------------------------------------------------------------- */

void destroy_attrs (befs_attr *attrs, int nattrs)
{
    int i;
    
    for (i=0; i<nattrs; i++)
    {
        free (attrs[i].name);
        free (attrs[i].value);
    }
    free (attrs);
}
