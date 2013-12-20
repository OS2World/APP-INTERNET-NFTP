#include <asvtools.h>

int main (int argc, char *argv[])
{
    time_t     t;
    struct tm  tm1;
    char       *p;
    
    if (argc != 2) error1 ("use: http_getdate <time string>");

    t = http_getdate (argv[1]);

    printf ("UTC: %ld\n", t);

    p = pretty_date (t);
    printf ("[%s]\n", p);
    p = pretty_difftime (t);
    printf ("[%s]\n", p);
    tm1 = *gmtime (&t);
    printf ("Year: %4d     Month:  %4d       Day: %4d\n"
            "Hour: %4d     Minute: %4d       Sec: %4d\n",
            tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,
            tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
    
    return 0;
}
