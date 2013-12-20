#include "manycode.h"

typedef struct
{
    unsigned char  c;
    Wchar_t        w;
}
conv_table;

typedef struct
{
    int        cp;
    conv_table *tbl;
}
cp_table;

extern cp_table CVT0[];
extern conv_table **CVT;
