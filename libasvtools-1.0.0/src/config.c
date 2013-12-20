#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asvtools.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define CFG_INTEGER   1
#define CFG_STRING    2
#define CFG_BOOLEAN   3
/* #define CFG_2INTEGERS 4 */
#define CFG_FLOAT     5
#define CFG_64BIT     6

#define CHUNK_ENTRIES_PER_SECTION    8
#define CHUNK_SECTIONS               32

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* -------------------------------------------------------------------- */

typedef struct
{
    char *name;
    int  type;
    union
    {
        int     integer;
        char    *string;
        int     boolean;
        double  floatv;
        int64_t intg64;
    } v;
}
cfg_item;

static int         Ng=0, **N, **Na, *S, *Sa;
static cfg_item    ***C;
static char        ***section_name;

static void cfg_grow (int group);
static void cfg_grow_section (int group, int is);
static void cfg_grow_groups (int group);
static void cfg_init (int group);
static int  cfg_lookup_section (int group, char *section);

/* -------------------------------------------------------------------- */

int cfg_write (int group, char *configfile)
{
    FILE   *fp;
    int    i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);

    fp = fopen (configfile, "w");
    if (fp == NULL) return -1;

    fprintf (fp, "# automatically read/written file; edit with great care\n\n");
    for (is=0; is<S[group]; is++)
    {
        if (N[group][is] == 0) continue;
        if (is != 0)
            fprintf (fp, "\n[%s]\n", section_name[group][is]);
        for (i=0; i<N[group][is]; i++)
        {
            switch (C[group][is][i].type)
            {
            case CFG_INTEGER:
                fprintf (fp, "%s=%u,%d\n", C[group][is][i].name, CFG_INTEGER, C[group][is][i].v.integer);
                break;
            case CFG_64BIT:
                fprintf (fp, "%s=%u,%s\n", C[group][is][i].name, CFG_64BIT, printf64(C[group][is][i].v.intg64));
                break;
            case CFG_STRING:
                fprintf (fp, "%s=%u,%s\n", C[group][is][i].name, CFG_STRING, C[group][is][i].v.string);
                break;
            case CFG_BOOLEAN:
                fprintf (fp, "%s=%u,%c\n", C[group][is][i].name, CFG_BOOLEAN, C[group][is][i].v.boolean ? '1' : '0');
                break;
            case CFG_FLOAT:
                fprintf (fp, "%s=%u,%f\n", C[group][is][i].name, CFG_FLOAT, C[group][is][i].v.floatv);
            }
        }
    }

    fclose (fp);
    return 0;
}

/* --------------------------------------------------------------------
 returns 0 if OK, -1 if read error */

int cfg_read (int group, char *configfile)
{
    FILE   *fp;
    char   buffer[8192], *var_name, *var_value, *p, *section;
    int    rc, type;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);

    /* check existence of file */
    if (access (configfile, R_OK) != 0) return -1;

    fp = fopen (configfile, "r");
    if (fp == NULL) return -1;

    section = NULL;
    while (1)
    {
        if (fgets (buffer, sizeof(buffer), fp) == NULL) break;
        str_strip (buffer, " \n\r");
        if (buffer[0] == '#' || buffer[0] == '\0' || buffer[0] == ' ') continue;
        /* check for section */
        if (buffer[0] == '[' && str_tailcmp (buffer, "]") == 0)
        {
            str_strip2 (buffer, "[]");
            if (section != NULL) free (section);
            section = strdup (buffer);
            continue;
        }
        rc = str_break_ini_line (buffer, &var_name, &var_value);
        if (rc == 0)
        {
            p = strchr (var_value, ',');
            if (p != NULL)
            {
                *p = '\0';
                type = atoi (var_value);
                switch (type)
                {
                case CFG_INTEGER:
                    cfg_set_integer (group, section, var_name, atoi (p+1));
                    break;
                case CFG_64BIT:
                    cfg_set_64bit (group, section, var_name, strtoq1 (p+1, NULL, 10));
                    break;
                case CFG_STRING:
                    cfg_set_string (group, section, var_name, p+1);
                    break;
                case CFG_BOOLEAN:
                    cfg_set_boolean (group, section, var_name, *(p+1) == '1');
                    break;
                case CFG_FLOAT:
                    cfg_set_float (group, section, var_name, atof(p+1));
                }
            }
            free (var_name); free (var_value);
        }
    }

    fclose (fp);
    if (section != NULL) free (section);
    return 0;
}

/* --------------------------------------------------------------------
 returns 0 if OK, -1 if exists and type is not CFG_STRING
 */

int cfg_set_string (int group, char *section, char *name, char *value)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
    {
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_STRING) return -1;
            if (C[group][is][i].v.string != NULL) free (C[group][is][i].v.string);
            C[group][is][i].v.string = strdup (value);
            return 0;
        }
    }

    /* not found; adding */
    if (N[group][is] == Na[group][is]) cfg_grow_section (group, is);
    C[group][is][N[group][is]].name = strdup (name);
    C[group][is][N[group][is]].type = CFG_STRING;
    C[group][is][N[group][is]].v.string = strdup (value);
    N[group][is]++;
    return 0;
}

/* --------------------------------------------------------------------
 returns 0 if OK, -1 if exists and type is not CFG_INTEGER
 */

int cfg_set_integer (int group, char *section, char *name, int value)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
    {
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_INTEGER) return -1;
            C[group][is][i].v.integer = value;
            return 0;
        }
    }

    /* not found; adding */
    if (N[group][is] == Na[group][is]) cfg_grow_section (group, is);
    C[group][is][N[group][is]].name = strdup (name);
    C[group][is][N[group][is]].type = CFG_INTEGER;
    C[group][is][N[group][is]].v.integer = value;
    N[group][is]++;
    return 0;
}

/* --------------------------------------------------------------------
 returns 0 if OK, -1 if exists and type is not CFG_64BIT
 */

int cfg_set_64bit (int group, char *section, char *name, int64_t value)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
    {
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_64BIT) return -1;
            C[group][is][i].v.intg64 = value;
            return 0;
        }
    }

    /* not found; adding */
    if (N[group][is] == Na[group][is]) cfg_grow_section (group, is);
    C[group][is][N[group][is]].name = strdup (name);
    C[group][is][N[group][is]].type = CFG_64BIT;
    C[group][is][N[group][is]].v.intg64 = value;
    N[group][is]++;
    return 0;
}

/* --------------------------------------------------------------------
 returns 0 if OK, -1 if exists and type is not CFG_BOOLEAN
 */

int cfg_set_boolean (int group, char *section, char *name, int value)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
    {
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_BOOLEAN) return -1;
            C[group][is][i].v.boolean = value;
            return 0;
        }
    }

    /* not found; adding */
    if (N[group][is] == Na[group][is]) cfg_grow_section (group, is);
    C[group][is][N[group][is]].name = strdup (name);
    C[group][is][N[group][is]].type = CFG_BOOLEAN;
    C[group][is][N[group][is]].v.boolean = value;
    N[group][is]++;
    return 0;
}

/* --------------------------------------------------------------------
 returns 0 if OK, -1 if exists and type is not CFG_FLOAT
 */

int cfg_set_float (int group, char *section, char *name, double value)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
    {
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_FLOAT) return -1;
            C[group][is][i].v.floatv = value;
            return 0;
        }
    }

    /* not found; adding */
    if (N[group][is] == Na[group][is]) cfg_grow_section (group, is);
    C[group][is][N[group][is]].name = strdup (name);
    C[group][is][N[group][is]].type = CFG_FLOAT;
    C[group][is][N[group][is]].v.floatv = value;
    N[group][is]++;
    return 0;
}

/* --------------------------------------------------------------------
 returns pointer to value if OK, NULL if name is not found or type is not CFG_STRING
 */

char *cfg_get_string (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_STRING) return "";
            return C[group][is][i].v.string;
        }
    return "";
}

/* --------------------------------------------------------------------
 returns value if OK, 0 if name is not found or type is not CFG_INTEGER
 */

int cfg_get_integer (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_INTEGER) return 0;
            return C[group][is][i].v.integer;
        }
    return 0;
}

/* --------------------------------------------------------------------
 returns value if OK, 0 if name is not found or type is not CFG_64BIT
 */

int64_t cfg_get_64bit (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_64BIT) return 0;
            return C[group][is][i].v.intg64;
        }
    return 0;
}

/* --------------------------------------------------------------------
 returns value if OK, 0 if name is not found or type is not CFG_FLOAT
 */

double cfg_get_float (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_FLOAT) return 0.0;
            return C[group][is][i].v.floatv;
        }
    return 0;
}

/* --------------------------------------------------------------------
 returns value if OK, FALSE if name is not found or type is not CFG_BOOLEAN
 */

int cfg_get_boolean (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type != CFG_BOOLEAN) return FALSE;
            return C[group][is][i].v.boolean;
        }
    return FALSE;
}

/* --------------------------------------------------------------------
 returns TRUE if name exists and is of type CFG_STRING, FALSE otherwise
 */

int cfg_check_string (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type == CFG_STRING) return TRUE;
            return FALSE;
        }
    return FALSE;
}

/* --------------------------------------------------------------------
 returns TRUE if name exists and is of type CFG_INTEGER, FALSE otherwise
 */

int cfg_check_integer (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type == CFG_INTEGER) return TRUE;
            return FALSE;
        }
    return FALSE;
}

/* --------------------------------------------------------------------
 returns TRUE if name exists and is of type CFG_INTEGER, FALSE otherwise
 */

int cfg_check_64bit (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type == CFG_64BIT) return TRUE;
            return FALSE;
        }
    return FALSE;
}

/* --------------------------------------------------------------------
 returns TRUE if name exists and is of type CFG_FLOAT, FALSE otherwise
 */

int cfg_check_float (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type == CFG_FLOAT) return TRUE;
            return FALSE;
        }
    return FALSE;
}

/* --------------------------------------------------------------------
 returns TRUE if name exists and is of type CFG_BOOLEAN, FALSE otherwise
 */

int cfg_check_boolean (int group, char *section, char *name)
{
    int   i, is;

    if (group >= Ng) cfg_grow_groups (group);
    if (C[group] == NULL) cfg_init (group);
    is = cfg_lookup_section (group, section);

    /* look through configuration for 'name' */
    for (i=0; i<N[group][is]; i++)
        if (strcmp (name, C[group][is][i].name) == 0)
        {
            if (C[group][is][i].type == CFG_BOOLEAN) return TRUE;
            return FALSE;
        }
    return FALSE;
}

/* -------------------------------------------------------------------- */

void cfg_destroy (int group)
{
    int i, is;
    
    for (is=0; is<S[group]; is++)
    {
        free (section_name[group][is]);
        for (i=0; i<N[group][is]; i++)
            if (C[group][is][i].type == CFG_STRING)
                free (C[group][is][i].v.string);
    }
    free (C[group]);
    free (N[group]);
    free (Na[group]);
    C[group] = NULL;
}

/* -------------------------------------------------------------------- */

static void cfg_init (int group)
{
    int  is;

    Sa[group] = CHUNK_SECTIONS;
    S[group] = 1;
    C[group] = xmalloc (sizeof (cfg_item *) * Sa[group]);
    N[group] = xmalloc (sizeof (int *) * Sa[group]);
    Na[group] = xmalloc (sizeof (int *) * Sa[group]);
    section_name[group] = xmalloc (sizeof (char *) * Sa[group]);
    for (is=0; is<Sa[group]; is++)
    {
        C[group][is] = NULL;
        N[group][is] = 0;
        Na[group][is] = 0;
        section_name[group][is] = NULL;
    }

    /* create unnamed section */
    C[group][0] = xmalloc (sizeof (cfg_item) * CHUNK_ENTRIES_PER_SECTION);
    N[group][0] = 0;
    Na[group][0] = CHUNK_ENTRIES_PER_SECTION;
}

/* -------------------------------------------------------------------- */

static void cfg_grow_section (int group, int is)
{
    C[group][is] = xrealloc (C[group][is], sizeof (cfg_item) * (Na[group][is]+CHUNK_ENTRIES_PER_SECTION));
    Na[group][is] += CHUNK_ENTRIES_PER_SECTION;
}

/* -------------------------------------------------------------------- */

static void cfg_grow (int group)
{
    C[group] = xrealloc (C[group], sizeof (cfg_item *) * (Sa[group]+CHUNK_SECTIONS));
    N[group] = xrealloc (N[group], sizeof (int *) * (Sa[group]+CHUNK_SECTIONS));
    Na[group] = xrealloc (Na[group], sizeof (int *) * (Sa[group]+CHUNK_SECTIONS));
    section_name[group] = xrealloc (section_name[group], sizeof (char *) * (Sa[group]+CHUNK_SECTIONS));
    Sa[group] += CHUNK_SECTIONS;
}

/* -------------------------------------------------------------------- */

static int  cfg_lookup_section (int group, char *section)
{
    int i;

    /* unnamed section? */
    if (section == NULL || section[0] == '\0') return 0;

    /* look among existing */
    for (i=1; i<S[group]; i++)
        if (strcmp (section_name[group][i], section) == 0) return i;

    /* not found */
    if (S[group] == Sa[group]-1) cfg_grow (group);
    C[group][S[group]] = xmalloc (sizeof (cfg_item) * CHUNK_ENTRIES_PER_SECTION);
    N[group][S[group]] = 0;
    Na[group][S[group]] = CHUNK_ENTRIES_PER_SECTION;
    section_name[group][S[group]] = strdup (section);
    S[group]++;
    return S[group]-1;
}

/* -------------------------------------------------------------------- */

static void cfg_grow_groups (int group)
{
    int i;

    if (Ng == 0)
    {
        Ng = max1 (4, group+1);
        N  = xmalloc (sizeof(void *) * Ng);
        Na = xmalloc (sizeof(void *) * Ng);
        S  = xmalloc (sizeof(void *) * Ng);
        Sa = xmalloc (sizeof(void *) * Ng);
        C  = xmalloc (sizeof(void *) * Ng);
        section_name = xmalloc (sizeof(void *) * Ng);
        for (i=0; i<Ng; i++) C[i] = NULL;
    }
    else
    {
        Ng = max1 (Ng*2, group);
        N  = xrealloc (N, sizeof(void *) * Ng);
        Na = xrealloc (Na, sizeof(void *) * Ng);
        S  = xrealloc (S, sizeof(void *) * Ng);
        Sa = xrealloc (Sa, sizeof(void *) * Ng);
        C  = xrealloc (C, sizeof(void *) * Ng);
        section_name = xrealloc (section_name, sizeof(void *) * Ng);
        for (i=Ng/2; i<Ng; i++) C[i] = NULL;
    }
}

