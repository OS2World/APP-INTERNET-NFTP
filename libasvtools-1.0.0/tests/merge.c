#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <asvtools.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#if 1
int a1[] = {1, 3, 8, 12, 14, 15, 18, 20};
int a2[] = {2, 3, 4, 5, 7, 12, 15, 15, 15, 15, 18};
int a3[] = {1, 2, 3, 4, 8, 12, 15, 18, 20, 21, 25};
#else
int a1[] = {1};
int a2[] = {2};
int a3[] = {1};
#endif

int nrep = 100;

int test_decode_speed (int N);
int test_vmerge (int in_vby, int n, int mtype, int prnt);
int test_psort (int N, int Np, int prnt);

int main (int argc, char *argv[])
{
    int    i, j, c, n1, *r, **arr, *nm, mode, N, Np, mtype, in_vby, prnt;

    optind  = 1;
    opterr  = 0;
    mode    = 's';
    mtype   = 1;
    N       = 1000;
    Np      = 10;
    in_vby  = FALSE;
    prnt    = FALSE;
    while ((c = getopt (argc, argv, "Mn:Op:PSr:RvVt:")) != -1)
    {
        switch (c)
        {
        case 'M':
            mode = 'm';
            break;

        case 'n':
            N = atoi (optarg);
            break;

        case 'p':
            Np = atoi (optarg);
            break;

        case 'P':
            prnt = TRUE;
            break;

        case 'r':
            nrep = atoi (optarg);
            break;

        case 'R':
#ifdef __FreeBSD__
            srandomdev ();
#else
            warning1 ("option -R works only on FreeBSD\n");
#endif
            break;

        case 'S':
            mode = 's';
            break;

        case 'V':
            mode = 'v';
            break;

        case 'v':
            in_vby = TRUE;
            break;

        case 't':
            mtype = atoi (optarg);
            break;

        case 'O':
            mode = 'o';
            break;

        default:
            error1 ("invalid option %c\n", c);
        }
    }
    argv += optind;
    argc -= optind;

    switch (mode)
    {
    case 'm':
        test_decode_speed (N);
        break;

    case 's':
        arr = xmalloc (sizeof(int *) * 4);
        nm  = xmalloc (sizeof(int) * 4);
        arr[0] = a1; nm[0] = sizeof(a1)/sizeof(int);
        arr[1] = a2; nm[1] = sizeof(a2)/sizeof(int);
        arr[2] = a3; nm[2] = sizeof(a3)/sizeof(int);
        arr[3] = NULL; nm[3] = 0;

        for (i=0; i<3; i++)
        {
            printf ("%d. ", i);
            for (j=0; j<nm[i]; j++)
                printf ("%d ", arr[i][j]);
            printf ("\n");
        }

        n1 = merge (mtype, 3, arr, nm, sizeof(int), &r, cmp_integers);
        printf ("merge returned %d\n", n1);
        if (n1 < 0) return 1;

        for (i=0; i<n1; i++) printf ("%d ", r[i]);
        printf ("\n");
        break;

    case 'v':
        test_vmerge (in_vby, N, mtype, prnt);
        break;

    case 'o':
        test_psort (N, Np, prnt);
    }

    printf ("done\n");

    return 0;
}

/* create a sorted list of unique integers */
char *new_list (int in_vby, u_int32_t *n, u_int32_t maxval)
{
    int       i;
    u_int32_t *a, n1, nb, last;
    char      *b;

    n1 = *n;
    a = xmalloc (sizeof (u_int32_t) * n1);
    for (i=0; i<n1; i++)
        a[i] = random () % maxval;

    qsort (a, n1, sizeof (u_int32_t), cmp_unsigned_integers);
    n1 = uniq (a, n1, sizeof (u_int32_t), cmp_unsigned_integers);
    *n = n1;

    if (!in_vby) return (char *)a;

    b = xmalloc (5 * n1);
    nb = 0;
    last = 0;
    for (i=0; i<n1; i++)
    {
        nb += vby_encode (b+nb, a[i]-last);
        last = a[i];
    }

    free (a);
    return b;
}

/* merge two delta-vby compressed lists of integers ('a' and 'b') into
 * new list (c). 'na' and 'nb' are number of integers in the lists 'a'
 * and 'b'. 'c' must be preallocated and its size in bytes is 'cs'.
 * when result won't fit into 'cs', the rest will be discarded.
 * on return, 'cs' will contain number of bytes unused in the buffer.
 * 'nc' will contain number of integers in the
 * resulting array. op=1 means intersection and op=2 means union of the lists.
 * duplicates are discarded when doing op=2. result is also delta-vby
 * compressed. returns 0 on success, negative value on error. when
 * resulting list is empty, 'nc' contains 0.
 * each input list is supposed to contain unique integers */
int vmerge2 (int op, char *a, int na, char *b, int nb,
             char *c, int *cs, int *nc)
{
    int       i, ni, no, cs1, nc1;
    int       na_bytes, nb_bytes, na_dec, nb_dec;
    u_int32_t da, db, d, last_d;

    cs1 = *cs;
    switch (op)
    {
    case 1:
        /* if one of the lists is empty, result is empty too */
        if (na == 0 || nb == 0)
        {
            *nc = 0;
            *cs = 0;
            return 0;
        }

        /* unimplemented */
        return -2;

    case 2:
        /* if one of the lists is empty, return another list */
        if (na == 0)
        {
            /* if both lists are empty, result is empty too */
            if (nb == 0)
            {
                *nc = 0;
                *cs = 0;
            }
            else
            {
                ni = 0;
                no = 0;
                for (i=0; i<nb; i++)
                {
                    if (cs1-no < 5) break;
                    ni += vby_decode (b+ni, &d);
                    no += vby_encode (c+no, d);
                }
                *nc = i;
                *cs = no;
            }
            return 0;
        }
        else if (nb == 0)
        {
            /* na cannot be 0! we have checked it already */
            ni = 0;
            no = 0;
            for (i=0; i<na; i++)
            {
                if (cs1-no < 5) break;
                ni += vby_decode (a+ni, &d);
                no += vby_encode (c+no, d);
            }
            *nc = i;
            *cs = no;
            return 0;
        }

        /* load first integers. they are not deltas */
        na_bytes = 0;
        na_dec = 1;
        na_bytes = vby_decode (a+na_bytes, &da);

        nb_bytes = 0;
        nb_dec = 1;
        nb_bytes = vby_decode (b+nb_bytes, &db);

        no = 0;
        nc1 = 0;
        last_d = 0;
        /* is there space in the result for another integer? */
        while (cs1-no > 5)
        {
            if (da < db)
            {
                no += vby_encode (c+no, da-last_d);
                last_d = da;
                nc1++;

                if (na_dec == na)
                {
                    if (cs1-no > 5)
                    {
                        /* copy the rest from 'b' */
                        no += vby_encode (c+no, db-last_d);
                        last_d = db;
                        nc1++;

                        while (cs1-no > 5 && nb_dec < nb)
                        {
                            nb_bytes += vby_decode (b+nb_bytes, &d);
                            nb_dec++;
                            db += d;
                            no += vby_encode (c+no, db-last_d);
                            last_d = db;
                            nc1++;
                        }
                    }
                    break;
                }

                na_bytes += vby_decode (a+na_bytes, &d);
                da += d;
                na_dec++;
            }
            else if (da > db)
            {
                no += vby_encode (c+no, db-last_d);
                last_d = db;
                nc1++;

                if (nb_dec == nb)
                {
                    if (cs1-no > 5)
                    {
                        /* copy the rest from 'a' */
                        no += vby_encode (c+no, da-last_d);
                        last_d = da;
                        nc1++;

                        while (cs1-no > 5 && na_dec < na)
                        {
                            na_bytes += vby_decode (a+na_bytes, &d);
                            na_dec++;
                            da += d;
                            no += vby_encode (c+no, da-last_d);
                            last_d = da;
                            nc1++;
                        }
                    }
                    break;
                }

                nb_bytes += vby_decode (b+nb_bytes, &d);
                db += d;
                nb_dec++;
            }
            else /* da == db */
            {
                no += vby_encode (c+no, da-last_d);
                last_d = da;
                nc1++;

                if (na_dec == na)
                {
                    if (cs1-no > 5)
                    {
                        /* copy the rest from 'b'. first item is skipped
                         * because it is equal to 'da'*/
                        while (cs1-no > 5 && nb_dec < nb)
                        {
                            nb_bytes += vby_decode (b+nb_bytes, &d);
                            nb_dec++;
                            db += d;
                            no += vby_encode (c+no, db-last_d);
                            last_d = db;
                            nc1++;
                        }
                    }
                    break;
                }
                else if (nb_dec == nb)
                {
                    if (cs1-no > 5)
                    {
                        /* copy the rest from 'a'. first item is skipped
                         * because it is equal to 'db'*/
                        while (cs1-no > 5 && na_dec < na)
                        {
                            na_bytes += vby_decode (a+na_bytes, &d);
                            na_dec++;
                            da += d;
                            no += vby_encode (c+no, da-last_d);
                            last_d = da;
                            nc1++;
                        }
                    }
                    break;
                }

                na_bytes += vby_decode (a+na_bytes, &d);
                da += d;
                na_dec++;

                nb_bytes += vby_decode (b+nb_bytes, &d);
                db += d;
                nb_dec++;
            }
        }

        *nc = nc1;
        *cs -= no;
        return 0;

    default:
        return -1;
    }
}

int test_vmerge (int in_vby, int n, int mtype, int prnt)
{
    int       i, j, rc;
    u_int32_t *au, *bu, *c1, na, nb, nc1, nc, n1, cs, da, db, d, last;
    char      *a, *b, *c;
    double    t1;

    printf ("will run %u repetitions\n", nrep);

    /* create arrays 'a' and 'b' */
    na = n;
    a = new_list (in_vby, &na, n*10);
    nb = n;
    b = new_list (in_vby, &nb, n*10);
    printf ("created two arrays: %u and %u elements\n", na, nb);

    /* do routine sort/uniq to produce result to compare with. we have
     * to unpack integers to do that */
    if (in_vby)
    {
        au = xmalloc (sizeof (u_int32_t) * na);
        bu = xmalloc (sizeof (u_int32_t) * nb);

        t1 = clock1 ();

        for (j=0; j<nrep; j++)
        {
            n1 = 0;
            da = 0;
            for (i=0; i<na; i++)
            {
                n1 += vby_decode (a+n1, &d);
                da += d;
                au[i] = da;
            }

            n1 = 0;
            db = 0;
            for (i=0; i<nb; i++)
            {
                n1 += vby_decode (b+n1, &d);
                db += d;
                bu[i] = db;
            }
        }
        t1 = clock1() - t1;
        printf ("decoding two arrays: %.3f mksec\n", t1/nrep*1000000.0);
    }
    else
    {
        au = (u_int32_t *)a;
        bu = (u_int32_t *)b;
    }

    if (prnt)
    {
        printf ("A (%u elems): ", na);
        for (i=0; i<na; i++)
            printf ("%u ", au[i]);
        printf ("\n");

        printf ("B (%u elems): ", nb);
        for (i=0; i<nb; i++)
            printf ("%u ", bu[i]);
        printf ("\n");
    }

    t1 = clock1 ();
    nc1 = na + nb;
    c1 = xmalloc (sizeof (u_int32_t) * nc1);
    for (i=0; i<nrep; i++)
    {
        nc1 = na + nb;
        memcpy (c1, au, sizeof (u_int32_t) * na);
        memcpy (c1 + na, bu, sizeof (u_int32_t) * nb);
        qsort (c1, nc1, sizeof (u_int32_t), cmp_unsigned_integers);
        nc1 = uniq (c1, nc1, sizeof (u_int32_t), cmp_unsigned_integers);
    }
    /* c1 now contains etalon merge result (in unpacked format) */
    t1 = clock1() - t1;
    printf ("%u elements in merged array\n", nc1);
    printf ("straight copy/sort/uniq in unpacked mode: %.3f mksecs\n",
            t1/nrep*1000000.0);

    if (prnt)
    {
        printf ("straight - %d elems:\n", nc1);
        for (i=0; i<nc1; i++)
            printf ("%u ", c1[i]);
        printf ("\n");
    }

    /* now merge using vmerge into c */
    cs = 5 * (na+nb) + 5;
    c = xmalloc (cs);
    t1 = clock1 ();
    for (i=0; i<nrep; i++)
    {
        cs = 5 * (na+nb) + 5;
        rc = vmerge2 (mtype, a, na, b, nb, c, &cs, &nc);
        if (rc != 0) error1 ("vmerge2() failed with rc=%d\n", rc);
    }
    t1 = clock1() - t1;
    printf ("%u elements in merged array\n", nc);
    printf ("%u bytes left out of %u bytes\n", cs, 5 * (na+nb) + 5);
    printf ("vmerge2(): %.3f mksecs\n", t1/nrep*1000000.0);

    if (prnt)
    {
        int n1, d, d1;

        printf ("vmerge2 - %d elems:\n", nc);
        n1 = 0;
        d = 0;
        for (i=0; i<nc; i++)
        {
            n1 += vby_decode (c+n1, &d1);
            d += d1;
            d1 = d;
            printf ("%u ", d1);
        }
        printf ("\n");
    }
    /* compare results of straight copy/sort/uniq with vmerge2() */
    if (nc1 != nc) printf ("nc (%u) != nc1 (%u)\n", nc, nc1);
    n1 = 0;
    last = 0;
    for (i=0; i<nc1; i++)
    {
        n1 += vby_decode (c+n1, &d);
        d += last;
        if (d != c1[i])
        {
            printf ("mismatch in element %u: straight %u, vmerge2 %u\n",
                    i, d, c1[i]);
            return 0;
        }
        last = d;
    }
    printf ("Success\n");

    return 0;
}

int test_decode_speed (int n)
{
    int     i, j, na, n1;
    char    *a;
    int32_t *au, da, d, sum = 0;
    double  t1;

    /* create array */
    na = n;
    a = new_list (TRUE, &na, n*10);
    printf ("created array of %u elements\n", na);

    au = xmalloc (sizeof (u_int32_t) * na);
    n1 = 0;
    da = 0;
    for (i=0; i<na; i++)
    {
        n1 += vby_decode (a+n1, &d);
        da += d;
        au[i] = da;
    }

    /* compute sum of an array */
    printf ("unpacked array, size %u bytes\n", na*sizeof(int32_t));
    t1 = clock1 ();
    for (j=0; j<nrep; j++)
    {
        sum = 0;
        for (i=0; i<na; i++)
            sum += au[i];
    }
    t1 = clock1 () - t1;
    printf ("%.3f mksec/sum/number; sum = %u\n", t1/(nrep*na)*1000000.0, sum);

    printf ("packed array, size %u bytes\n", n1);
    t1 = clock1 ();
    for (j=0; j<nrep; j++)
    {
        sum = 0;
        n1 = 0;
        da = 0;
        for (i=0; i<na; i++)
        {
            n1 += vby_decode (a+n1, &d);
            da += d;
            sum += da;
        }
    }
    t1 = clock1 () - t1;
    printf ("%.3f mksec/sum/number; sum = %u\n", t1/(nrep*na)*1000000.0, sum);

    return 0;
}

typedef struct
{
    int   docid;
    float weight;
    int   ballast;
}
hit_t;

static int cmp_hit (const void *e1, const void *e2)
{
    hit_t *h1, *h2;

    h1 = (hit_t *) e1;
    h2 = (hit_t *) e2;

    if (h1->weight > h2->weight) return +1;
    if (h1->weight < h2->weight) return -1;
    return h1->docid - h2->docid;
}

int test_psort (int N, int Np, int prnt)
{
    int     i, x, N1;
    hit_t   *a, *ac, *at;
    double  t1;

    /* create array */
    t1 = clock1 ();
    a = xmalloc (sizeof (hit_t) * N);
    for (i=0; i<N; i++)
    {
        x = random ();
        a[i].docid = x;
        a[i].weight = (float) x;
        a[i].ballast = 1;
    }
    printf ("created array of %u elements in %.3f sec\n", N, clock1()-t1);

    /* make a copy and sort it (fully) */
    ac = xmalloc (sizeof (hit_t) * N);
    memcpy (ac, a, sizeof (hit_t) * N);
    t1 = clock1 ();
    qsort (ac, N, sizeof (hit_t), cmp_hit);
    warning1 ("full sort: %.3f sec\n", clock1()-t1);
    ac = xrealloc (ac, sizeof (hit_t) * Np);

    /* make a copy and sort it (partially) */
    at = xmalloc (sizeof (hit_t) * N);
    memcpy (at, a, sizeof (hit_t) * N);
    t1 = clock1 ();
    N1 = psort (at, N, sizeof (hit_t), Np, cmp_hit);
    warning1 ("partial sort: %.3f sec, %u elements\n", clock1()-t1, N1);
    for (i=0; i<min1(Np, N1); i++)
    {
        if (at[i].docid != ac[i].docid)
        {
            int j;

            printf ("FAILED at position %u (%u != %u)\n",
                    i, at[i].docid, ac[i].docid);
            for (j=max1(i-5, 0); j<min1(Np, i+5); j++)
            {
                printf ("i=%u full=%u partial=%u\n", j, ac[j].docid, at[j].docid);
            }
            break;
        }
    }

    /* make a copy and sort it (partially) */
    memcpy (at, a, sizeof (hit_t) * N);
    t1 = clock1 ();
    N1 = psort1 (at, N, sizeof (hit_t), Np, cmp_hit);
    warning1 ("partial sort via heap: %.3f sec, %u elements\n", clock1()-t1, N1);
    for (i=0; i<min1(Np, N1); i++)
    {
        int j;

        if (at[i].docid != ac[i].docid)
        {
            printf ("FAILED at position %u (%u != %u)\n",
                    i, at[i].docid, ac[i].docid);
            for (j=max1(i-5, 0); j<min1(Np, i+5); j++)
            {
                printf ("i=%u full=%u partial=%u\n", j, ac[j].docid, at[j].docid);
            }
            break;
        }
    }

    return 0;
}
