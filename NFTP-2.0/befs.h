typedef struct
{
    int   type;
    char  *name;
    char  *value;
    int   length;
}
befs_attr;

extern char *ATTR_SUFFIX;

int bfs_fetch  (char *filename, befs_attr **attrs);
int bfs_attach (char *filename, befs_attr *attrs, int nattrs);

int write_attrfile (char *filename, befs_attr *attrs, int nattrs);
int read_attrfile  (char *filename, befs_attr **attrs);

void destroy_attrs (befs_attr *attrs, int nattrs);

