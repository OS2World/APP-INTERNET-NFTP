
int FTPS_delete (int index);
int FTPS_get_item (int index, int n, int *type, int *rights, int *size, int *timestamp);
char *FTPS_get_hostname (int index, int n);
char *FTPS_get_pathname (int index, int n);
char *FTPS_get_filename (int index, int n);
char *FTPS_get_login (int index, int n);
char *FTPS_get_password (int index, int n);
