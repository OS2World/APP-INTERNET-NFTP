#include <fly/fly.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "nftp.h"

char  *nls_strings[MAX_NLS_STRINGS];

static void perr (char *reason, char *string);

struct strname {char *v; int  n;};
static struct strname strings[] = {
#include "nls.cns"
    {"trailer", 0}};

static int N = sizeof(strings) / sizeof(struct strname) - 1;

extern int bpc;

/* ------------------------------------------------------------- */

int nls_init (char *system_libpath, char *user_libpath)
{
    char  nls_filename[1024], *language;
    char  *p, *p1, *p2, *p3, *p4, *p5, *p6;
    char  line[256], *B;
    char  var_name[128], var_value[16384];
    FILE  *f;
    int   i, l, lB, error_count=0;

    // clear arrays
    for (i = 0; i < MAX_NLS_STRINGS; i++)
        nls_strings[i] = "undefined";

    // find out what language user wants
    if (cmdline.english)
    {
        language = "english";
    }
    else
    {
        language = cfg_get_string (CONFIG_NFTP, fl_opt.platform_nick, "language");
        str_strip (language, " ");
    }
    
    // Try $user_libpath directory
    strcpy (nls_filename, user_libpath);
    str_cats (nls_filename, language);
    strcat (nls_filename, ".nls");
    if (access (nls_filename, R_OK) == 0) goto Found;

    // Try $user_libpath/nls directory
    strcpy (nls_filename, user_libpath);
    str_cats (nls_filename, "nls");
    str_cats (nls_filename, language);
    strcat (nls_filename, ".nls");
    if (access (nls_filename, R_OK) == 0) goto Found;

    // Try $system_libpath directory
    strcpy (nls_filename, system_libpath);
    str_cats (nls_filename, language);
    strcat (nls_filename, ".nls");
    if (access (nls_filename, R_OK) == 0) goto Found;

    // Try $system_libpath/nls directory
    strcpy (nls_filename, system_libpath);
    str_cats (nls_filename, "nls");
    str_cats (nls_filename, language);
    strcat (nls_filename, ".nls");
    if (access (nls_filename, R_OK) == 0) goto Found;

    // try ../nls subdirectory
    strcpy (nls_filename, "../nls");
    str_cats (nls_filename, language);
    strcat (nls_filename, ".nls");
    if (access (nls_filename, R_OK) == 0) goto Found;
    
    fly_error ("\nFailed to load NLS file for \'%s\' language.\n", language);

Found:

    if (fl_opt.has_console && !fl_opt.initialized)
        fly_ask_ok (0, "loading %s......\n", nls_filename);

    // read file into memory, discard comments
    f = fopen (nls_filename, "r");
    l = file_length (fileno(f));
    B = malloc (l*2);
    B[0] = '\0';
    lB = 0;
    while (fgets (line, sizeof(line), f) != NULL)
    {
        if (line[0] == '#') continue; // skip comments
        p = line + strlen(line) - 1;
        while (p >= line && (*p == ' ' || *p == '\r' || *p == '\n'))
            *p-- = '\0';
        if (line[0] == '\0') continue; // skip empty lines
        l = strlen (line);
        strcpy (B+lB, line);
        // insert blank between the lines
        strcpy (B+lB+l, " ");
        lB += l + 1;
    }
    fclose (f);

    // now analyze it
    p = B;
    while (1)
    {
        // clear things just to make sure
        var_name[0]  = '\0';
        var_value[0] = '\0';

        // knife group inside {}
        
        // find opening { . skip whitespace for this
        p1 = p;
        while (*p1 == ' ' && *p1 != '\0') p1++;
        if (*p1 == '\0') break;  // EOF
        // check whether we have some text before { - sure sign of error
        if (*p1 != '{') perr ("stray text outside group:", p1);
        // look for closing }
        p2 = str_index1 (p1+1, '}');
        if (p2 == NULL) perr ("no matching } for :", p1);
        // separate our string
        *p2 = '\0';
        // check for stray { in the group
        if (str_index1 (p1+1, '{') != NULL) perr ("stray { in :", p1);
        // skip the brace, advance the starting point
        p1++;       p = p2+1;
        
        // extract variable name
        
        // skip leading spaces
        p3 = p1;
        while (*p3 == ' ' && *p3 != '\0') p3++;
        if (*p3 == '\0') perr ("empty group: ", p1);
        // find delimiting space
        p4 = p3 + strcspn (p3, " \"");
        if (p4 == NULL) perr ("no delimiting space after variable name: ", p3);
        // place a stopper and copy to destination
        *p4 = '\0';
        strcpy (var_name, p3);

        // extract variable value

        p5 = p4+1;
        while (1)
        {
            // skip leading spaces
            while (*p5 == ' ' && *p5 != '\0') p5++;
            if (*p5 == '\0') break;
            // check for obligatory "
            if (*p5 != '\"') perr ("value does not start with double quote: ", p5);
            p5++;
            // find ending "
            p6 = str_index1 (p5, '\"');
            if (p6 == NULL) perr ("ending quote is missing: ", p5);
            // place a stopper and copy to destination
            *p6 = '\0';
            strcat (var_value, p5);
            // add separating '\n'
            strcat (var_value, "\n");
            p5 = p6+1;
        }
        if (var_value[0] == '\0') perr ("no value has been specified for ", var_name);
        // delete last added \n
        var_value[strlen(var_value)-1] = '\0';
        
        // put it into nls_strings array

        for (i=0; i<N; i++)
            if (strcmp (strings[i].v, var_name) == 0)
            {
                if (strcmp (nls_strings[strings[i].n], "undefined") != 0)
                    perr ("duplicated variable: ", var_name);
                nls_strings[strings[i].n] = chunk_add (var_value);
                break;
            }
        if (i == N)
        {
            fly_ask_ok (0, "bogus variable: %s\n", var_name);
            error_count++;
        }
    }
    free (B);

    // check for missing definitions
    for (i=0; i<N; i++)
    {
        if (strcmp (nls_strings[strings[i].n], "undefined") == 0)
        {
            fly_ask_ok (0, "%s is missing\n", strings[i].v);
            error_count++;
        }
    }

    if (error_count && fl_opt.has_console && !fl_opt.initialized)
    {
        fprintf (stderr, "Press ENTER...");
        fgetc (stdin);
    }

    return 0;
}

/* ------------------------------------------------------------- */

static void perr (char *reason, char *string)
{
    if (strlen (string) > 100)
    {
        string[90] = '\0';
        strcat (string, "...");
    }
    fly_error ("error parsing language file: %s%s\n", reason, string);
}
