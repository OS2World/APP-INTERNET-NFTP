#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static void parse_time (char *s, int *hour, int *min, int *sec);
static int guess_year (struct tm t, struct tm now);

/* parses string representation of date, setting corresponding
 fields in the tm struct. string is not necessary needed length
 DATE_FMT_1  1   27-SEP-1996
 DATE_FMT_2  2   27/09/96
 DATE_FMT_3  3   09/27/96
 DATE_FMT_4  4   Sep 27 14:52 or Sep 27  1996
 DATE_FMT_5  5   96/09/27
 DATE_FMT_6  6   Jul 17 02:08:00 1999
 DATE_FMT_7  7   YYYYMMDDHHMMSS.sss
 DATE_FMT_8  8   2000/01/17
 DATE_FMT_9  9   Jul 17 02:08 1999
 */
time_t parse_date_time (int fmt, char *ds, char *ts)
{
    time_t       t0;
    struct   tm  now, t;
    char         buf[21], *p1, *p2, sep;
    int          l;

    time (&t0);
    now = *gmtime (&t0); /* this is UTC! */

    t.tm_hour  = 0;
    t.tm_min   = 0;
    t.tm_sec   = 0;
    t.tm_mday  = 1;
    t.tm_mon   = 0;
    t.tm_year  = now.tm_year;
    t.tm_wday  = 0;
    t.tm_yday  = 0;
    t.tm_isdst = 0;
    
    switch (fmt)
    {
    case DATE_FMT_1: /* 27-SEP-1996 */
        if (strlen (ds) < 11) break;
        memcpy (buf, ds, 11); buf[11] = '\0';
        if (str_numchars (buf, '-') != 2) break;
        buf[2] = '\0';        buf[6] = '\0';
        t.tm_mday = atoi (buf);
        t.tm_mon = txt2mon (buf+3);
        t.tm_year = atoi (buf+7) - 1900;
        break;

    case DATE_FMT_2: /* 27/09/96 or 7/09/96 or 27-09-96 or 7-09-96 */
    case DATE_FMT_3: /* 09/27/96 or 9/27/96 or 09-27-96 or 9-27-96 */
        l = strlen (ds);
        if (l < 7) break;
        l = min1 (l, 8);
        memcpy (buf, ds, l); buf[l] = '\0';
        if (str_numchars (buf, '/') == 2)
            sep = '/';
        else
            if (str_numchars (buf, '-') == 2)
                sep = '-';
            else
                break;
        p1 = strchr (buf, sep);   p2 = strchr (p1+1, sep);
        if (p1 == NULL || p2 == NULL) break;
        *p1 = '\0';               *p2 = '\0';
        if (fmt == DATE_FMT_2)
        {
            t.tm_mday = atoi (buf);
            t.tm_mon = atoi (p1+1);
        }
        if (fmt == DATE_FMT_3)
        {
            t.tm_mday = atoi (p1+1);
            t.tm_mon = atoi (buf);
        }
        if (t.tm_mon > 0) t.tm_mon--;
        t.tm_year = atoi (p2+1);
        if (t.tm_year < 50) t.tm_year += 100;   /* Y2K compliance */
        break;

    case DATE_FMT_4: /* Sep 27 14:52 or Sep 27  1996 */
        if (strlen (ds) < 12) break;
        memcpy (buf, ds, 12); buf[12] = '\0';
        buf[3] = '\0';
        buf[6] = '\0';
        t.tm_mon = txt2mon (buf);
        t.tm_mday = atoi (buf+4);
        if (strchr (buf+7, ':') == NULL)
        {
            t.tm_year = atoi (buf+7) - 1900;
        }
        else
        {
            t.tm_year = guess_year (t, now);
            /* compute difference in months */
            /* monthdiff = (now.tm_mon+now.tm_year*12) - (t.tm_mon+t.tm_year*12); */
            /* if ( */
            /*    now.tm_mon > 6 && */
            /*    abs(t.tm_mon-now.tm_mon) > 6  || */
            /*    (t.tm_mon == now.tm_mon && t.tm_mday > now.tm_mday+1)) */
            /*    t.tm_year--; */
            parse_time (buf+7, &t.tm_hour, &t.tm_min, &t.tm_sec);
        }
        break;
        
    case DATE_FMT_5: /* 96/09/27 or 96-09-27 */
        if (strlen (ds) < 8) break;
        memcpy (buf, ds, 8); buf[8] = '\0';
        buf[2] = '\0'; buf[5] = '\0';
        t.tm_year = atoi (buf);
        if (t.tm_year < 50) t.tm_year += 100;   /* Y2K compliance */
        t.tm_mon = atoi (buf+3);
        if (t.tm_mon > 0) t.tm_mon--;
        t.tm_mday = atoi (buf+6);
        break;

    case DATE_FMT_6: /* Jul 17 02:08:00 1999 */
        if (strlen (ds) < 20) break;
        memcpy (buf, ds, 20); buf[20] = '\0';
        buf[3] = '\0';
        t.tm_mon = txt2mon (buf);
        buf[6] = '\0';
        t.tm_mday = atoi (buf+4);
        t.tm_year = atoi (buf+16) - 1900;
        buf[15] = '\0';
        parse_time (buf+7, &t.tm_hour, &t.tm_min, &t.tm_sec);
        break;

    case DATE_FMT_7: /* YYYYMMDDHHMMSS.sss */
        if (strlen (ds) < 14) break;
        memcpy (buf, ds+0, 4); buf[4] = '\0';
        t.tm_year = atoi (buf)-1900;
        memcpy (buf, ds+4, 2); buf[2] = '\0';
        t.tm_mon = atoi (buf)-1;
        memcpy (buf, ds+6, 2); buf[2] = '\0';
        t.tm_mday = atoi (buf);
        memcpy (buf, ds+8, 2); buf[2] = '\0';
        t.tm_hour = atoi (buf);
        memcpy (buf, ds+10, 2); buf[2] = '\0';
        t.tm_min = atoi (buf);
        memcpy (buf, ds+12, 2); buf[2] = '\0';
        t.tm_sec = atoi (buf);
        break;

    case DATE_FMT_8: /* 2000/01/17 */
        if (strlen (ds) < 10) break;
        memcpy (buf, ds, 10); buf[10] = '\0';
        buf[4] = '\0'; buf[7] = '\0';
        t.tm_year = atoi (buf)-1900;
        t.tm_mon = atoi (buf+5);
        if (t.tm_mon > 0) t.tm_mon--;
        t.tm_mday = atoi (buf+8);
        break;

    case DATE_FMT_9: /* Jul 17 02:08 1999 */
        if (strlen (ds) < 17) break;
        memcpy (buf, ds, 17); buf[17] = '\0';
        buf[3] = '\0';
        t.tm_mon = txt2mon (buf);
        buf[6] = '\0';
        t.tm_mday = atoi (buf+4);
        t.tm_year = atoi (buf+13) - 1900;
        buf[12] = '\0';
        parse_time (buf+7, &t.tm_hour, &t.tm_min, &t.tm_sec);
        break;

    case DATE_FMT_10: /* YYYY[[[[[MM]DD]HH]MM]SS] */
        l = strlen (ds);
        if (l < 4 || l > 14) break;
        memcpy (buf, ds+0, 4); buf[4] = '\0';
        t.tm_year = atoi (buf)-1900;
        if (l >= 6)
        {
            memcpy (buf, ds+4, 2); buf[2] = '\0';
            t.tm_mon = atoi (buf)-1;
            if (l >= 8)
            {
                memcpy (buf, ds+6, 2); buf[2] = '\0';
                t.tm_mday = atoi (buf);
                if (l >= 10)
                {
                    memcpy (buf, ds+8, 2); buf[2] = '\0';
                    t.tm_hour = atoi (buf);
                    if (l >= 12)
                    {
                        memcpy (buf, ds+10, 2); buf[2] = '\0';
                        t.tm_min = atoi (buf);
                        if (l == 14)
                        {
                            memcpy (buf, ds+12, 2); buf[2] = '\0';
                            t.tm_sec = atoi (buf);
                        }
                    }
                }
            }
        }
        break;

    }

    if (ts != NULL)
        parse_time (ts, &t.tm_hour, &t.tm_min, &t.tm_sec);

    if (t.tm_year < 70)
    {
        t.tm_year = 70; t.tm_mon = 0; t.tm_mday = 1;
        t.tm_hour = 0;  t.tm_min = 0; t.tm_sec  = 0;
    }

    return timegm1 (&t);
}

/* converts time broken representation into number of seconds. no
 timezone conversion is assumed */
time_t timegm1 (struct tm *gmtimein)
{
    time_t    t;
    struct tm tm1, tm2, *ptm;

    /* convert given time to UTC seconds assuming it is local time */
    tm1 = *gmtimein;
    tm1.tm_isdst = 0;
    t = mktime (&tm1);
    if (t == 4294967295UL) return 0;

    /* convert obtained time back to UTC */
    ptm = gmtime (&t);
    if (ptm == NULL) return 0;
    tm2 = *ptm;
    tm2.tm_isdst = 0;
    
    t += t - mktime (&tm2);
    
    return t;
}

/* parses string representation of time, setting corresponding
 fields in the tm struct. string is not necessary needed length
 12:30 or 12:30:55 or 8:30 or 8:20:55 */
static void parse_time (char *s, int *hour, int *min, int *sec)
{
    char         buf[16], *p1, *p2;
    int          l;

    *hour = 0;
    *min  = 0;
    *sec  = 0;

    l = strlen (s);
    if (l < 4) return;
    l = min1 (l, 8);
    memset (buf, ' ', sizeof (buf));
    memcpy (buf, s, l); buf[l] = '\0';

    /* hour */
    p1 = strchr (buf, ':');
    if (p1 == NULL) return;
    *p1 = '\0';
    *hour = atoi (buf);
    if (strchr (p1+1, 'P') != NULL) *hour += 12;

    /* minutes */
    p2 = strchr (p1+1, ':');
    if (p2 != NULL) *p2 = '\0';
    *min = atoi (p1+1);

    /* seconds if present */
    if (p2 == NULL) return;
    *(p2+3) = '\0';
    *sec = atoi (p2+1);

    /* check validity */
    if (*sec > 59)  *sec = 0;
    if (*min > 59)  *min = 0;
    if (*hour > 23) *hour = 0;
}

time_t gm2local (time_t t1)
{
    struct tm   tm1;

    if (t1 <= 0) return 0;
    
    tm1 = *localtime (&t1);
    /* tm1.tm_isdst = 0; */
    return timegm1 (&tm1);
}

time_t local2gm (time_t t1)
{
    time_t tdiff;
    
    if (t1 == 0) return 0;
    
    tdiff = gm2local (t1) - t1;
    
    return t1 - tdiff;
}

#define dd(y,m,d) ((now.tm_year-(y))*365 + (now.tm_mon-(m))*30 + now.tm_mday-(d)-30)

static int guess_year (struct tm t, struct tm now)
{
    int d1, d2, d3;

    /* printf ("\nt:   %d %d %d", t.tm_year, t.tm_mon, t.tm_mday); */
    /* printf ("\nnow: %d %d %d", now.tm_year, now.tm_mon, now.tm_mday); */
    /* printf ("\n-1: %d", dd(t.tm_year-1, t.tm_mon, t.tm_mday)); */
    /* printf ("\n  : %d", dd(t.tm_year  , t.tm_mon, t.tm_mday)); */
    /* printf ("\n+1: %d", dd(t.tm_year+1, t.tm_mon, t.tm_mday)); */

    d1 = abs(dd(t.tm_year-1, t.tm_mon, t.tm_mday));
    d2 = abs(dd(t.tm_year  , t.tm_mon, t.tm_mday));
    d3 = abs(dd(t.tm_year+1, t.tm_mon, t.tm_mday));

    if (d1 < d2 && d1 < d3) return t.tm_year-1;
    if (d3 < d1 && d3 < d2) return t.tm_year+1;
    return t.tm_year;
}

/* converts month name into number. returns January when unknown.
 months start from 0 (January - 0) */
int txt2mon (char *m)
{
    int    month = 0;
    char   *m1;

    m1 = strdup (m);
    str_lower (m1);

    if (memcmp (m1, "jan", 3) == 0) month = 0;
    else if (memcmp (m1, "feb", 3) == 0) month = 1;
    else if (memcmp (m1, "mar", 3) == 0) month = 2;
    else if (memcmp (m1, "apr", 3) == 0) month = 3;
    else if (memcmp (m1, "may", 3) == 0) month = 4;
    else if (memcmp (m1, "jun", 3) == 0) month = 5;
    else if (memcmp (m1, "jul", 3) == 0) month = 6;
    else if (memcmp (m1, "aug", 3) == 0) month = 7;
    else if (memcmp (m1, "sep", 3) == 0) month = 8;
    else if (memcmp (m1, "oct", 3) == 0) month = 9;
    else if (memcmp (m1, "nov", 3) == 0) month = 10;
    else if (memcmp (m1, "dec", 3) == 0) month = 11;

    free (m1);
    return month;
}

/* ------------------------------------------------------------- */

char *make_time (unsigned long time)
{
    static char buf[10];
    unsigned long   hr, mn, sec, i0, i1, i2;

    time = min1 (999L*3600+59L*60+59L, time);

    hr  = time/3600;
    mn  = (time - hr*3600)/60;
    sec = time - hr*3600 - mn*60;

    if (hr != 0)
    {
       split_to_digits (hr, i0, i1, i2);
       buf[0] = (i0==0) ? ' ' : '0'+i0;
       buf[1] = (i0==0 && i1==0) ? ' ' : '0'+i1;
       buf[2] = '0'+i2;
       buf[3] = ':';
    }
    else
       memset (buf, ' ', 4);

    split_to_digits (mn, i0, i1, i2);
    buf[4] = (i1+hr==0) ? ' ' : '0'+i1;
    buf[5] = '0'+i2;
    buf[6] = ':';

    split_to_digits (sec, i0, i1, i2);
    buf[7] = '0'+i1;
    buf[8] = '0'+i2;

    return buf;
}

/* returns static buffer containing local time in pretty format */
char *datetime (void)
{
   static char buffer[30];
   time_t      timer;
   struct tm   *tmdt;

   time (&timer);
   tmdt = localtime (&timer);
   if (tmdt == NULL)
     *buffer = 0;
   else
       snprintf1 (buffer, sizeof(buffer), "%02u/%02u/%04u %02u:%02u:%02u",
                  tmdt->tm_mday, tmdt->tm_mon+1, 1900+tmdt->tm_year,
                  tmdt->tm_hour, tmdt->tm_min, tmdt->tm_sec);

   return buffer;
}

/*
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *	This product includes software developed by Powerdog Industries.
 * 4. The name of Powerdog Industries may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define asizeof(a)	(sizeof (a) / sizeof ((a)[0]))

struct dtconv
{
    char	*abbrev_month_names[12];
    char	*month_names[12];
    char	*abbrev_weekday_names[7];
    char	*weekday_names[7];
    char	*time_format;
    char	*sdate_format;
    char	*dtime_format;
    char	*am_string;
    char	*pm_string;
    char	*ldate_format;
};

static struct dtconv En_US =
{
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
    { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" },
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
    { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" },
    "%H:%M:%S",
    "%m/%d/%y",
    "%a %b %e %T %Z %Y",
    "AM",
    "PM",
    "%A, %B, %e, %Y"
};

char *str_ptime (char *buf, char *fmt, struct tm *tm)
{
    unsigned char  c, *buf1, *ptr;
    int	           i, len;

    buf1 = (unsigned char *)buf;
    len = 0;
    ptr = (unsigned char *)fmt;
    while (*ptr != 0)
    {
        if (*buf1 == 0)
            break;

        c = *ptr++;

        if (c != '%')
        {
            if (isspace(c))
                while (*buf1 != 0 && isspace(*buf1))
                    buf1++;
            else if (c != *buf1++)
                return 0;
            continue;
        }

        c = *ptr++;
        switch (c)
        {
        case 0:
        case '%':
            if (*buf1++ != '%')
                return 0;
            break;

        case 'C':
            buf1 = (unsigned char*)str_ptime((char *)buf1, En_US.ldate_format, tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'c':
            buf1 = (unsigned char*)str_ptime((char *)buf1, "%x %X", tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'D':
            buf1 = (unsigned char*)str_ptime((char *)buf1, "%m/%d/%y", tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'R':
            buf1 = (unsigned char*)str_ptime((char *)buf1, "%H:%M", tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'r':
            buf1 = (unsigned char*)str_ptime((char *)buf1, "%I:%M:%S %p", tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'T':
            buf1 = (unsigned char*)str_ptime((char *)buf1, "%H:%M:%S", tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'X':
            buf1 = (unsigned char*)str_ptime((char *)buf1, En_US.time_format, tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'x':
            buf1 = (unsigned char*)str_ptime((char *)buf1, En_US.sdate_format, tm);
            if (buf1 == 0)
                return 0;
            break;

        case 'j':
            if (!isdigit(*buf1))
                return 0;

            for (i = 0; *buf1 != 0 && isdigit(*buf1); buf1++)
            {
                i *= 10;
                i += *buf1 - '0';
            }
            if (i > 365)
                return 0;

            tm->tm_yday = i;
            break;

        case 'M':
        case 'S':
            if (*buf1 == 0 || isspace(*buf1))
                break;

            if (!isdigit(*buf1))
                return 0;

            for (i = 0; *buf1 != 0 && isdigit(*buf1); buf1++)
            {
                i *= 10;
                i += *buf1 - '0';
            }
            if (i > 59)
                return 0;

            if (c == 'M')
                tm->tm_min = i;
            else
                tm->tm_sec = i;

            if (*buf1 != 0 && isspace(*buf1))
                while (*ptr != 0 && !isspace(*ptr))
                    ptr++;
            break;

        case 'H':
        case 'I':
        case 'k':
        case 'l':
            if (!isdigit(*buf1))
                return 0;

            for (i = 0; *buf1 != 0 && isdigit(*buf1); buf1++)
            {
                i *= 10;
                i += *buf1 - '0';
            }
            if (c == 'H' || c == 'k') {
                if (i > 23)
                    return 0;
            } else if (i > 11)
                return 0;

            tm->tm_hour = i;

            if (*buf1 != 0 && isspace(*buf1))
                while (*ptr != 0 && !isspace(*ptr))
                    ptr++;
            break;

        case 'p':
            len = strlen(En_US.am_string);
            if (strnicmp1 ((char *)buf1, En_US.am_string, len) == 0)
            {
                if (tm->tm_hour > 12)
                    return 0;
                if (tm->tm_hour == 12)
                    tm->tm_hour = 0;
                buf1 += len;
                break;
            }

            len = strlen(En_US.pm_string);
            if (strnicmp1 ((char *)buf1, En_US.pm_string, len) == 0)
            {
                if (tm->tm_hour > 12)
                    return 0;
                if (tm->tm_hour != 12)
                    tm->tm_hour += 12;
                buf1 += len;
                break;
            }

            return 0;

        case 'A':
        case 'a':
            for (i = 0; i < asizeof(En_US.weekday_names); i++)
            {
                len = strlen(En_US.weekday_names[i]);
                if (strnicmp1 ((char *)buf1,
                               En_US.weekday_names[i],
                               len) == 0)
                    break;

                len = strlen(En_US.abbrev_weekday_names[i]);
                if (strnicmp1 ((char *)buf1,
                               En_US.abbrev_weekday_names[i],
                               len) == 0)
                    break;
            }
            if (i == asizeof(En_US.weekday_names))
                return 0;

            tm->tm_wday = i;
            buf1 += len;
            break;

        case 'd':
        case 'e':
            if (!isdigit(*buf1))
                return 0;

            for (i = 0; *buf1 != 0 && isdigit(*buf1); buf1++)
            {
                i *= 10;
                i += *buf1 - '0';
            }
            if (i > 31)
                return 0;

            tm->tm_mday = i;

            if (*buf1 != 0 && isspace(*buf1))
                while (*ptr != 0 && !isspace(*ptr))
                    ptr++;
            break;

        case 'B':
        case 'b':
        case 'h':
            for (i = 0; i < asizeof(En_US.month_names); i++)
            {
                len = strlen(En_US.month_names[i]);
                if (strnicmp1 ((char *)buf1,
                               En_US.month_names[i],
                               len) == 0)
                    break;

                len = strlen(En_US.abbrev_month_names[i]);
                if (strnicmp1 ((char *)buf1,
                               En_US.abbrev_month_names[i],
                               len) == 0)
                    break;
            }
            if (i == asizeof(En_US.month_names))
                return 0;

            tm->tm_mon = i;
            buf1 += len;
            break;

        case 'm':
            if (!isdigit(*buf1))
                return 0;

            for (i = 0; *buf1 != 0 && isdigit(*buf1); buf1++)
            {
                i *= 10;
                i += *buf1 - '0';
            }
            if (i < 1 || i > 12)
                return 0;

            tm->tm_mon = i - 1;

            if (*buf1 != 0 && isspace(*buf1))
                while (*ptr != 0 && !isspace(*ptr))
                    ptr++;
            break;

        case 'Y':
        case 'y':
            if (*buf1 == 0 || isspace(*buf1))
                break;

            if (!isdigit(*buf1))
                return 0;

            for (i = 0; *buf1 != 0 && isdigit(*buf1); buf1++)
            {
                i *= 10;
                i += *buf1 - '0';
            }
            if (i < 50) i += 100;

            if (c == 'Y')
                i -= 1900;
            if (i < 0)
                return 0;

            tm->tm_year = i;

            if (*buf1 != 0 && isspace(*buf1))
                while (*ptr != 0 && !isspace(*ptr))
                    ptr++;
            break;
        }
    }

    return (char *)buf1;
}

#define HTTP_TIME_FORMAT "%a, %d %b %Y %T"  /* Fri, 28 Feb 2003 10:14:35 GMT */
#define HTTP_TIME_FORMAT2 "%a, %d-%b-%y %T" 
#define HTTP_TIME_FORMAT3 "%a %b %d %T %Y" /* Mon Feb 02 15:00:32 2004 */
#define STR_LEN		1024		/* size of common strings */

/* retrieves date from HTTP time string */
time_t http_getdate (char *datestring)
{
    struct tm   tm;
    char *p;

    memset (&tm, 0, sizeof (struct tm));
    p = datestring;
    while (isspace(*p)) p++;

    if (str_ptime (p, HTTP_TIME_FORMAT, &tm) == NULL)
    {
        if (str_ptime (p, HTTP_TIME_FORMAT2, &tm) == NULL)
        {
            if (str_ptime (p, HTTP_TIME_FORMAT3, &tm) == NULL)
            {
                return 0;
            }
        }
    }

    if (tm.tm_year < 0)
        tm.tm_year += 1900;
    
    return timegm1 (&tm);
}

#define MAX_BUFS 8

static char buffers[MAX_BUFS][128];
static int  ib=0;

/* pretty-printing for the date. returns pointer to statically
 allocated, NULL-terminated buffer. format: 31/12/2000 23:59:59 */
char *pretty_date (time_t t)
{
    char  *buf;
    struct tm *tm1;

    buf = buffers[ib];
    ib = (ib == MAX_BUFS-1) ? 0 : ib+1;

    tm1 = gmtime (&t);
    snprintf1 (buf, 128, "%02d/%02d/%04d %02d:%02d:%02d",
               tm1->tm_mday, tm1->tm_mon+1, tm1->tm_year+1900,
               tm1->tm_hour, tm1->tm_min, tm1->tm_sec);

    return buf;
}

/* pretty-printing for the date only. returns pointer to statically
 allocated, NUL-terminated buffer. format: 31/12/2000 */
char *pretty_dateonly (time_t t)
{
    char  *buf;
    struct tm *tm1;

    buf = buffers[ib];
    ib = (ib == MAX_BUFS-1) ? 0 : ib+1;

    tm1 = gmtime (&t);
    snprintf1 (buf, 128, "%02d/%02d/%04d",
               tm1->tm_mday, tm1->tm_mon+1, tm1->tm_year+1900);

    return buf;
}

#define YEAR_SEC  (365*24*60*60)
#define MONTH_SEC (30*24*60*60)
#define DAY_SEC   (24*60*60)
#define HOUR_SEC  (60*60)
#define MIN_SEC   (60)
#define SEC_SEC   1

char *pretty_difftime (time_t t)
{
    char  *buf, buf1[32], *p;

    buf = buffers[ib];
    ib = (ib == MAX_BUFS-1) ? 0 : ib+1;

    if (t == 0)
    {
        strcpy (buf, "0s");
        return buf;
    }

    buf[0] = '\0';
    p = buf;
    if (t < 0)
    {
        strcat (p++, "-");
        t = -t;
    }

    if (t > YEAR_SEC)
    {
        snprintf1 (buf1, sizeof(buf1), "%dy", (int)t/YEAR_SEC);
        t = t % YEAR_SEC;
        strcat (p, buf1);
        p += strlen (buf1);
    }

    if (t > MONTH_SEC)
    {
        if (p > buf+1) strcat (p++, " ");
        snprintf1 (buf1, sizeof(buf1), "%dmn", (int)t/MONTH_SEC);
        t = t % MONTH_SEC;
        strcat (p, buf1);
        p += strlen (buf1);
    }

    if (t > DAY_SEC)
    {
        if (p > buf+1) strcat (p++, " ");
        snprintf1 (buf1, sizeof(buf1), "%dd", (int)t/DAY_SEC);
        t = t % DAY_SEC;
        strcat (p, buf1);
        p += strlen (buf1);
    }

    if (t > HOUR_SEC)
    {
        if (p > buf+1) strcat (p++, " ");
        snprintf1 (buf1, sizeof(buf1), "%dh", (int)t/HOUR_SEC);
        t = t % HOUR_SEC;
        strcat (p, buf1);
        p += strlen (buf1);
    }

    if (t > MIN_SEC)
    {
        if (p > buf+1) strcat (p++, " ");
        snprintf1 (buf1, sizeof(buf1), "%dm", (int)t/MIN_SEC);
        t = t % MIN_SEC;
        strcat (p, buf1);
        p += strlen (buf1);
    }

    if (t > SEC_SEC)
    {
        if (p > buf+1) strcat (p++, " ");
        snprintf1 (buf1, sizeof(buf1), "%ds", (int)t/SEC_SEC);
        t = t % MIN_SEC;
        strcat (p, buf1);
        p += strlen (buf1);
    }

    return buf;
}

