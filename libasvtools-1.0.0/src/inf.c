#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* ------------------------------------------------------------------------------ */

typedef struct
{
    char *name;
    char *value;
    int  section;
}
cfg_item;

static cfg_item    *C = NULL;
static char *      *S = NULL;
static int         NC, NS;
static int	   infIS=-1;
static int	   infIV=-1;


/* ------------------------------------------------------------------------------ */

int infLoad (char *filename)
{
    FILE   *fp;
    char   buffer[4096];
    int    rc, section, NSa, NCa;

    if (C != NULL) infFree ();

    /* check existence of file */
    if (access (filename, R_OK) != 0) return -1;
    
    fp = fopen (filename, "r");
    if (fp == NULL) return -1;

    NSa = 100;    NS = 0;
    S = malloc (sizeof (char *) * NSa);
    NCa = 100;    NC = 0;
    C = malloc (sizeof (cfg_item) * NCa);
    
    section = -1;
    while (1)
    {
        if (fgets (buffer, sizeof(buffer), fp) == NULL) break;
        str_strip2 (buffer, " \n\r");
        if (buffer[0] == ';' || buffer[0] == '#' || 
            buffer[0] == '\0' || buffer[0] == ' ') continue;
        /* check for section */
        if (buffer[0] == '[' && str_tailcmp (buffer, "]") == 0)
        {
            str_strip2 (buffer, "[]");
            S[NS] = strdup (buffer);
            section = NS++;
            if (NS == NSa-1)
            {
                NSa += 100;
                S = realloc (S, sizeof (char *) * NSa);
            }
        }
        else
        {
            if (section == -1) continue; /* skip junk before first section */
            rc = str_break_ini_line (buffer, &(C[NC].name), &(C[NC].value));
            if (rc) continue;
            C[NC++].section = section;
            if (NC == NCa-1)
            {
                NCa += 100;
                C = realloc (C, sizeof (cfg_item) * NCa);
            }
        }
    }

    fclose (fp);
    
    S = realloc (S, sizeof (char *) * (NS+1));
    C = realloc (C, sizeof (cfg_item) * (NC+1));

    /*
    for (i=0; i<NS; i++)
        printf ("section %2d: [%s]\n", i, S[i]);
    for (i=0; i<NC; i++)
        printf ("item %2d: [%s]%s=%s\n", i, S[C[i].section], C[i].name, C[i].value);
    fgets (buffer, sizeof(buffer), stdin); */
    
    return 0;
}

/* ------------------------------------------------------------------------------ */

void infFree (void)
{
    int    i;

    for (i=0; i<NC; i++)
    {
        free (C[i].name), free (C[i].value);
    }
    free (C);
    C = NULL;
    
    for (i=0; i<NS; i++)
        free (S[i]);
    free (S);
    S = NULL;
}

/* ------------------------------------------------------------------------------ */

static int find_variable (char *section, char *variable)
{
    int  i, s;
    
    /* look up section */
    for (s=0; s<NS; s++)
        if (strcmp (section, S[s]) == 0) break;
    if (s == NS) return -1;

    /* look up variable */
    for (i=0; i<NC; i++)
        if (C[i].section == s && strcmp (C[i].name, variable) == 0) break;
    if (i == NC) return -1;

    return i;
}

/* ------------------------------------------------------------------------------ */
char*  infIterateSection() {
	infIS++;
	return ( infIS<NS ) ? S[infIS] : NULL; 
}
/* ------------------------------------------------------------------------------ */
char*  infIterateVariable(char *section, char **option) {
	int s;
    	for (s=0; s<NS; s++)
     	   	if (strcmp (section, S[s]) == 0) break;
    	if (s == NS) return NULL;

	for (infIV++;  infIV<NC; infIV++)
		if (C[infIV].section == s) {
			*option=xstrdup(C[infIV].value);
    			if ((*option)[0] == '\"') str_strip2 (*option, "\"");
			return C[infIV].name;
		}

	return NULL;
}
/* ------------------------------------------------------------------------------ */
void  infStartIteratorSection() {
	infIS=-1;
}
/* ------------------------------------------------------------------------------ */
void  infStartIteratorVariable() {
	infIV=-1;
}
/* ------------------------------------------------------------------------------ */

int infGetInteger (char *section, char *variable, int *value)
{
    int  i;

    i = find_variable (section, variable);
    if (i == -1) return -1;

    *value = atoi (C[i].value);
    
    return 0;
}

/* ------------------------------------------------------------------------------ */

int infGetFloat (char *section, char *variable, float *value)
{
    int  i;

    i = find_variable (section, variable);
    if (i == -1) return -1;

    *value = atof (C[i].value);
    
    return 0;
}

/* ------------------------------------------------------------------------------ */

int infGetDouble (char *section, char *variable, double *value)
{
    int  i;

    i = find_variable (section, variable);
    if (i == -1) return -1;

    *value = atof (C[i].value);
    
    return 0;
}

/* ------------------------------------------------------------------------------ */

int infGetString (char *section, char *variable, char **value)
{
    int i;

    i = find_variable (section, variable);
    if (i == -1) return -1;

    *value = strdup (C[i].value);
    if ((*value)[0] == '\"') str_strip2 (*value, "\"");
    
    return 0;
}

/* ------------------------------------------------------------------------------ */

int infGetBoolean (char *section, char *variable, int *value)
{
    int i;

    i = find_variable (section, variable);
    if (i == -1) return -1;

    if (stricmp1 (C[i].value, "yes") == 0 || stricmp1 (C[i].value, "on") == 0 || strcmp (C[i].value, "1") == 0)
        *value = TRUE;
    else if (stricmp1 (C[i].value, "no") == 0 || stricmp1 (C[i].value, "off") == 0 || strcmp (C[i].value, "0") == 0)
        *value = FALSE;
    else
        return -1;
    
    return 0;
}

/* ------------------------------------------------------------------------------ */

int infGetHexbyte (char *section, char *variable, char *value)
{
    int  i;

    i = find_variable (section, variable);
    if (i == -1) return -1;

    str_strip2 (C[i].value, " ");
    if (strlen (C[i].value) != 2) return -1;
    if (!isxdigit((int)(C[i].value[0])) || !isxdigit((int)(C[i].value[1]))) return -1;
    
    *value = hex2dec (C[i].value[0]) * 16 + hex2dec (C[i].value[1]);
    
    return 0;
}

/* breaks typical line of "aaa=bbb" kind to two components (name and value),
 both malloc()ed. line is unchanged. returns 0 if success, -1 if error
 ('=' not found in the string) */
int str_break_ini_line (char *line, char **var_name, char **var_value)
{
    char  *p;
    int   ln;

    p = strchr (line, '=');
    if (p == NULL) return -1;

    /* create variable name */
    ln = p - line;
    *var_name = malloc (ln+1);
    strncpy (*var_name, line, ln);
    (*var_name)[ln] = '\0';
    str_strip2 (*var_name, " \t");

    /* create variable value */
    *var_value = strdup (p+1);
    str_strip2 (*var_value, " \t");
    
    return 0;
}
