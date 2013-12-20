#ifndef _FTPSEARCH_H_INCLUDED_
#define _FTPSEARCH_H_INCLUDED_

int  ftpsearch_find (char *s, url_t *u);
int  ftpsearch_recall (url_t *u);
int  ftpsearch_servers (void);
void ftpsearch_load_servers (void);
void ftpsearch_save_servers (void);

#endif
