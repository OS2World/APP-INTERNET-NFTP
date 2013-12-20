#include <fly/fly.h>

#include <string.h>
#include <stdlib.h>

#include "nftp.h"
#include "auxprogs.h"

static void displayhelp (char *s[]);

void help (int nh)
{
    char    **s, *b, *p;
    int     i, lines = 0;

    // make a copy
    b = malloc (strlen (MSG(nh)));
    strcpy (b, MSG(nh));
    
    // count lines in help screen and allocate buffer
    p = b;
    do
    {
        if (*p=='\n' || *p==0) lines++;
    }
    while (*p++ != 0);
    s = (char **)malloc ((lines+2)*sizeof(char *));
    
    str_translate (b, '\n', '\0');
    p = b;
    for (i=0; i<lines; i++)
    {
        s[i] = p;
        p += strlen (p) + 1;
    }
    s[i] = NULL;

    // display buffer
    displayhelp (s);
    
    free (s);
    free (b);
}

static void displayhelp (char *s[])
{
    char **p=s, buf[256], *savbuf;
    int  fl, nl, cl, ll, pg, key, line;
    int  c1, c2, r1, r2, c;

    // save screen
    savbuf = video_save (0, 0, video_vsize()-1, video_hsize()-1);
    menu_disable ();
    c = video_cursor_state (0);

    // count number of lines
    nl = 0;
    while (*p++) nl++;


    // main cycle: redraw, then wait for key, then again
    fl = 0;
    do
    {
        // set up initial values
        c1 = 3;        c2 = video_hsize()-3;
        r1 = 1;        r2 = video_vsize()-2;
        pg = r2-r1;
        ll = max1 (0, nl+1-pg);

        // prepare screen
        for (line=r1; line<=r2; line++)
            video_put_n_attr (options.attr_help, c2-c1+1, line, c1);
        fly_drawframe (r1, c1, r2, c2);
        video_put (MSG(M_HELP_ESC_TO_EXIT), r1, c1+4);

        for (line=r1+1; line <= r2-1; line++)
        {
            cl = line-r1-1 + fl;
            memset (buf, ' ', sizeof (buf));
            if (cl < nl) strncpy (buf, s[cl], c2-c1-1);
            video_put_str (buf, c2-c1-1, line, c1+1);
        }
        video_update (0);

        key = getmessage (-1);
        if (IS_KEY (key))
        {
            switch (key)
            {
            case _Up:    if (fl) fl--; break;
            case _Down:  if (fl < ll) fl++; break;
            case _Home:  fl = 0; break;
            case _End:   fl = ll; break;
            case _PgUp:  fl = max1 (0, fl-pg); break;
            case _Space:
            case _PgDn:  fl = min1 (ll, fl+pg); break;
            case _Esc:   break;
            default:     break;
            }
        }
        if (IS_MOUSE (key))
        {
            if (MOU_EVTYPE(key) == MOUEV_B2SC) key = _Esc;
        }
    }
    while (key != _Esc);

    // restore screen
    video_cursor_state (c);
    menu_enable ();
    video_restore (savbuf);
    video_update (0);
}
