#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manycode.h"
#include "ucs_tables.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* here are actual conversion tables built into the library */

conv_table iso8859_1[] =
{
    {0, 0}
};

/* table of tables containing pointers to conversion tables */

#include "unicode.tab"

conv_table **CVT = (void *)0;
