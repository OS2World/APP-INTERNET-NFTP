#include <asvtools.h>

int main (int argc, char *argv[])
{
    int res;
    char **list, buffer[8192];
    double t1;
    fastlist_t *fl;
   /* fast_catlist_t *cl; */

    if (argc != 2) error1 ("usage: %s exclude-file\n", argv[0]);

    warning1 ("reading ruleset\n");
    list = read_list (argv[1]);
    if (list == NULL) error1 ("failed to load pattern list from %s\n", argv[1]);

    fl = prepare_fastlist (list);
    /*
    printf ("shortest simple rule: %d\n", fl->shortest);
    for (i=0; i<fl->n_simple; i++)
    {
        printf ("%d %d %s\n",
                fl->rules[fl->simple[i]].type,
                fl->rules[fl->simple[i]].category,
                fl->rules[fl->simple[i]].rule);
    }
    */
    /*
    for (i=0; i<fl->n; i++)
    {
        printf ("%d %d %s\n", fl->rules[i].type, fl->rules[i].category,
                fl->rules[i].rule);
                }
                */
   /* for (i=0; i<fl->n_simple; i++) printf ("SIMPLE: %s\n", fl->simple[i]); */
   /* for (i=0; i<fl->n_complex; i++) printf ("COMPLEX: %s\n", fl->complex[i]); */

   /* cl = prepare_catlist (list); */
    /*
    for (i=0; i<cl->n_simple; i++)
        printf ("SIMPLE: %d.%s\n", cl->simple[i].category, cl->simple[i].rule);
    for (i=0; i<cl->n_coll; i++)
    {
        printf ("COMPLEX: %d (%d rules)\n",
                cl->colls[i].coll_id, cl->colls[i].nrules);
        for (j=0; j<cl->colls[i].nrules; j++)
        {
            printf ("  %s\n", cl->colls[i].rules[j]);
        }
    }
    */
    /*
    printf ("slow:\n");
    t1 = clock1 ();
    res = check_list (list, argv[2]);
    t1 = clock1 () - t1;

    if (res)
        printf ("%s is included (%.3f sec)\n", argv[2], t1);
    else
        printf ("%s is excluded (%.3f sec)\n", argv[2], t1);
        */

    while ((fgets (buffer, sizeof(buffer), stdin) != NULL))
    {
        str_strip (buffer, " \r\n");
        if (normalize_url2 (buffer) < 0)
        {
            printf ("bad url: %s\n", buffer);
            continue;
        }
        t1 = clock1 ();
        res = check_fastlist (fl, buffer);
        t1 = clock1 () - t1;

        if (res)
            printf ("%s is included (%.3f sec)\n", buffer, t1);
        else
            printf ("%s is excluded (%.3f sec)\n", buffer, t1);
    }

    return 0;
}
