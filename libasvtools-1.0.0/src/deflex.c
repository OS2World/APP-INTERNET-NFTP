#include "deflex.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

char *lextype_descriptions[] =
{
    "",
    "Latin word",
    "Cyrillic word",
    "Word",
    "Email",
    "URL",
    "Host",
    "Scientific notation",
    "VERSION",
    "Part of hyphenated word",
    "Cyrillic part of hyphenated word",
    "Latin part of hyphenated word",
    "Space symbols",
    "HTML Tag",
    "HTTP head",
    "Hyphenated word",
    "Latin hyphenated word",
    "Cyrillic hyphenated word",
    "URI",
    "File or path name",
    "Decimal notation",
    "Signed integer",
    "Unsigned integer",
    "HTML Entity",
    "Mix of alphanumeric and special chars"
};

char *lextype_names[] =
{
    "NONE",
    "LATWORD",
    "CYRWORD",
    "UWORD",
    "EMAIL",
    "URL",
    "HOST",
    "FLOAT",
    "VERSION",
    "HYP-PART",
    "HYP-CYRPART",
    "HYP-LATPART",
    "SPACE",
    "TAG",
    "PROTO",
    "HYPHWORD",
    "HYPHLATWORD",
    "HYPHCYRWORD",
    "URI",
    "PATHNAME",
    "DECIMAL",
    "SIGNED",
    "INTEGER",
    "ENTITY",
    "COMPLEXWORD"
};

int num_lextypes (void)
{
    return NUM_LEXTYPES;
}
