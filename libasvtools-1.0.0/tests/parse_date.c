#include <asvtools.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    time_t     t;
    struct tm  tm1;
    int        fmt;
    
    if (argc != 3 && argc != 1)
        error1 ("use: parse_date <fmt-number> <time string>");

    if (argc == 3)
    {
        fmt = atoi (argv[1]);
        fprintf (stdout, "format number: %d\ntime string:   %s\n", fmt, argv[2]);
        t = parse_date_time (fmt, argv[2], NULL);
    }
    else
    {
        t = time (NULL);
    }

    fprintf (stdout, "input time: %lu\n", t);

    tm1 = *gmtime (&t);
    fprintf (stdout, "%2u:%02u:%02u %2u/%2u/%4u\n",
             tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
             tm1.tm_mday, tm1.tm_mon+1, tm1.tm_year+1900);

    t = local2gm (t);
    fprintf (stdout, "converted to GM from local: %lu\n", t);

    tm1 = *gmtime (&t);
    fprintf (stdout, "%2u:%02u:%02u %2u/%2u/%4u\n",
             tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
             tm1.tm_mday, tm1.tm_mon+1, tm1.tm_year+1900);

    t = gm2local (t);
    fprintf (stdout, "converted to local from GM: %lu\n", t);

    tm1 = *gmtime (&t);
    fprintf (stdout, "%2u:%02u:%02u %2u/%2u/%4u\n",
             tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
             tm1.tm_mday, tm1.tm_mon+1, tm1.tm_year+1900);

    return 0;
}
