
#define SHAREWARE_PAUSE   10
#define SHAREWARE_TRIAL   60

enum {LIC_NO=0, LIC_VERIFIED=1, LIC_DELAYED=2};

typedef struct
{
    int   status;
    int   version, no, quantity;
    char  *type, *name, *sign;
    int   issue, expiry;
    int   n_ip_masks, n_host_masks;
    char  **ip_masks, **host_masks;
}
license_t;

extern license_t License;

void check_licensing (char *user_path, char *sys_path);
