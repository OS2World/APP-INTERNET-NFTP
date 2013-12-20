#ifndef _HISTORY_H_INCLUDED_
#define _HISTORY_H_INCLUDED_

int history_select (url_t *u);
int history_add (url_t *u);
int history_nickname (char *nickname, url_t *u);
int history_usage (void);

#endif
