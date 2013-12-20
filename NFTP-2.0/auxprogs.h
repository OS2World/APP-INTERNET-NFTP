#ifndef AUXPROGS_H_INCLUDED
#define AUXPROGS_H_INCLUDED

#include <stdio.h>

#define EF_INTEGER   1
#define EF_STRING    2
#define EF_BOOLEAN   3
#define EF_SEPARATOR 4

typedef struct
{
    char *banner;
    int  type; // 1 -- integer, 2 -- string, 3 -- boolean
    union
    {
        int  integer;
        char string[1024];
        int  boolean;
    }
    value;
    int flags;
}
entryfield_t;

/* lineinput flags */
#define LI_RECOGNIZE_TABS   0x0001 // react to Tab/Shift-Tab keys, returning PRESSED_TAB/PRESSED_SHIFTTAB
#define LI_INVISIBLE        0x0002 // display asterisks instead of string (for passwords)
#define LI_USE_ARROWS       0x0004 // make Down equivalent to Tab and Up - Shift-Tab

/* ---------------------------------------------------------------------
 multiline entry field
 ----------------------------------------------------------------------- */
int mle (int numfields, char **banners, char **buffers, int flags, char *helpmsg);
int mle2 (int numfields, entryfield_t *f, char *help1, int help2);

int lineinput (char *buffer, int buffer_length, int vis_length,
               char attribute, int srow, int scol, int flags, int help1);

int entryfield (char *banner, char *buffer, int buf_len, int flags);

int  Edit_Site (url_t *u, int allow_psw);
void dmsg (char *format, ...);
void ViewArray (char *s[], int nl, char *hdr);
char *mon2txt (int month);
int  file_compare (__const__ void *elem1, __const__ void *elem2);
//int  choose_local_drive (int current_drive);
void browse_buffer (char *s, int len, char *title);
void put_time (unsigned long time, int row, int col);
FILE *try_open (char *name, char *flags);
void Bell (int melody);
void tmpnam1 (char *s);
int  isindexfile (char *name);
int  breakflags (char *s);
char *makeflags (int flags);
char *scramble (char *s);
char *unscramble (char *s);
int is_absolute (char *path);
void dump_dirs (int nsite);
int is_8bit (char *s);
void ask_refresh (int nsite, char *fn);
int delete_subdir (char *path, int *noisy);
void try_set_local_cursor (int pnl, char *name);
void try_set_remote_cursor (int nsite, char *name);
void sort_remote_files (int nsite);
void sort_local_files (int pnl);
void adjust_cursor (directory *D, char *filename);
void set_marks (int nsite, char *dirname);
void remove_marks (int nsite, char *dirname);
int  is_walked (int nsite, int nd, int nf);
void set_dir_size (int nsite, int d, int f);
int  find_directory (int nsite, char *s);
void gather_marked (int nsite, int sub_only, int *marked_no, int *marked_dirs, int64_t *marked_size);
int localdir (directory *D, char *dirname);
void lcache_free (void);
void lcache_scandir (char *dirname);
void quicksearch (directory *d);
int create_directory (char *dirname);
int refresh (int panel);

#endif
