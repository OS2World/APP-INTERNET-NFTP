#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <asvtools.h>
#include <manycode.h>

#include <string.h>
#include <locale.h>
#include <stdlib.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* ---------------------------------------------------------------------- */

enum {MODE_PRINT_TRIGRAMS, MODE_TRIGRAM_STATISTICS};

/* trigram integer is:
 char1*256*256 + char2*256 + char3 */
 typedef int trigram_t;
 typedef struct
 {
     int trigram;
     int n;
 }
 trgstat_t;

/* ---------------------------------------------------------------------- */

int compute_trigrams (char *s, trigram_t **trg);
int print_trigrams (FILE *fp, trigram_t *trg, int ntrg);
int cmp_trgstat (const void *e1, const void *e2);
int empty_accumulator (trigram_t *tr, int ntr, trgstat_t **trgstat, int *ntrgstat);
int merge_frequency_tables (trgstat_t **trgstat, int *ntrgstat,
                            trgstat_t *ts2, int nts2);

/* ---------------------------------------------------------------------- */

int main (int argc, char *argv[])
{
    char      buf[8192], *progname, **w;
    int       i, j, ntrg, c, mode, ntr, ntr_a, ntrgstat, nw, ntr_total, ln;
    int       compact_stats, cyrillic_only, minhits;
    trigram_t *trg, *tr;
    trgstat_t *trgstat;
    double    t1;

    setlocale (LC_CTYPE, "");
    setlocale (LC_COLLATE, "");

    mode = MODE_PRINT_TRIGRAMS;
    compact_stats = FALSE;
    cyrillic_only = FALSE;
    minhits = 0;

    progname = strdup (argv[0]);
    optind  = 1;
    opterr  = 0;
    while ((c = getopt (argc, argv, "chm:M:")) != -1)
    {
        switch (c)
        {
        case 'c':
            compact_stats = TRUE;
            break;

        case 'h':
            cyrillic_only = TRUE;
            break;

        case 'm':
            minhits = atoi (optarg);
            break;

        case 'M':
            switch (optarg[0])
            {
            case 'P': mode = MODE_PRINT_TRIGRAMS; break;
            case 'S': mode = MODE_TRIGRAM_STATISTICS; break;
            }
            break;
        }
    }
    argv += optind;
    argc -= optind;

    /* mode-specific initializations */
    ntr = 0;
    ntr_a = 0;
    ntr_total = 0;
    tr = NULL;
    switch (mode)
    {
    case MODE_TRIGRAM_STATISTICS:
        /* allocate trigram accumulator */
        ntr = 0;
        ntr_a = 10*1024*1024;
        tr = xmalloc (sizeof (trigram_t) * ntr_a);
        /* set up empty trigram statistics accumulator */
        trgstat = xmalloc (sizeof(trgstat_t) * 1);
        ntrgstat = 0;
        ntr_total = 0;
        break;
    }

    t1 = clock1 ();
    while (fgets (buf, sizeof(buf), stdin) != NULL)
    {
        switch (mode)
        {
        case MODE_PRINT_TRIGRAMS:
            str_strip (buf, " \n\r");
            strlwr (buf);
            ntrg = compute_trigrams (buf, &trg);
            printf ("%s = ", buf);
            print_trigrams (stdout, trg, ntrg);
            printf ("\n");
            free (trg);
            break;

        case MODE_TRIGRAM_STATISTICS:
            str_strip (buf, " .,\n\r");
            strlwr (buf);
            if (strlen (buf) < 2) continue;

            nw = str_words (buf, &w, WSEP_LETTERS);
            for (i=0; i<nw; i++)
            {
                ln = strlen (w[i]);

                /* ignore too short words */
                if (ln < 2) continue;

                /* ignore words containing latin characters */
                if (cyrillic_only &&
                    strcspn (w[i], "abcdefghijklmnopqrstuvwxyz") != ln) continue;

                ntrg = compute_trigrams (w[i], &trg);

                /* handle accumulator overflow */
                if (ntrg + ntr > ntr_a)
                {
                    empty_accumulator (tr, ntr, &trgstat, &ntrgstat);
                    ntr = 0;
                }

                /* copy trigrams to common buffer */
                memcpy (tr+ntr, trg, sizeof(trigram_t)*ntrg);
                ntr += ntrg;
                ntr_total += ntrg;
                free (trg);
            }
            free (w);
            break;
        }
    }
    warning1 (" %.3f sec\n", clock1() - t1);

    /* mode-specific results */
    switch (mode)
    {
    case MODE_TRIGRAM_STATISTICS:
        empty_accumulator (tr, ntr, &trgstat, &ntrgstat);
        free (tr); /* this one consumes a lot of memory */

        /* kill trigrams which have less than minhits occurences */
        if (minhits > 1)
        {
            for (i=0, j=0; i<ntrgstat; i++)
            {
                if (trgstat[i].n < minhits) continue;
                trgstat[j++] = trgstat[i];
            }
            warning1 ("%d trigrams remain of %d after filtering (minhits=%d)\n",
                      j, ntrgstat, minhits);
            ntrgstat = j;
        }

        /* sort trigram frequency table and print it */
        if (!compact_stats)
            qsort (trgstat, ntrgstat, sizeof(trgstat_t), cmp_trgstat);
        for (i=0; i<ntrgstat; i++)
        {
            if (compact_stats)
            {
                printf ("{%d, %d},\n", trgstat[i].trigram, trgstat[i].n);
            }
            else
            {
                printf ("{%9d, %.15f} /* ", trgstat[i].trigram,
                        (double)trgstat[i].n/(double)ntr_total);
                print_trigrams (stdout, &trgstat[i].trigram, 1);
                printf (" %d */\n", trgstat[i].n);
            }
        }
        warning1 ("\n%d trigrams were analyzed\n", ntr_total);
        break;
    }

    return 0;
}

/* ---------------------------------------------------------------------- */

/* abcdef _ab abc bcd cde def ef_ */
int compute_trigrams (char *s, trigram_t **trg)
{
    int           i, ntrg1;
    trigram_t     *trg1;
    unsigned char *s1;

    s1 = s;
    ntrg1 = strlen (s1);
    trg1 = xmalloc (sizeof (trigram_t) * ntrg1);

    trg1[0] = s1[0]*256 + s1[1];
    for (i=1; i<ntrg1-1; i++)
    {
        trg1[i] = s1[i-1]*256*256 + s1[i]*256 + s1[i+1];
    }
    trg1[ntrg1-1] = s1[ntrg1-1-1]*256*256 + s1[ntrg1-1]*256;

    *trg = trg1;
    return ntrg1;
}

/* ---------------------------------------------------------------------- */

int print_trigrams (FILE *fp, trigram_t *trg, int ntrg)
{
    int i, c;

    if (ntrg > 1) fprintf (fp, "[");
    for (i=0; i<ntrg; i++)
    {
        fprintf (fp, "%d:", trg[i]);

        c = (trg[i] / (256*256)) % 256;
        if (c == 0) fprintf (fp, "_");
        else        fprintf (fp, "%c", c);

        c = (trg[i] / 256) % 256;
        fprintf (fp, "%c", c);

        c = trg[i] % 256;
        if (c == 0) fprintf (fp, "_");
        else        fprintf (fp, "%c", c);

        if (i != ntrg-1) fprintf (fp, ", ");
    }
    if (ntrg > 1) fprintf (fp, "]");

    return 0;
}

/* ---------------------------------------------------------------------- */

int cmp_trgstat (const void *e1, const void *e2)
{
    trgstat_t *ts1, *ts2;

    ts1 = (trgstat_t *)e1;
    ts2 = (trgstat_t *)e2;

    return - (ts1->n - ts2->n);
}

/* ---------------------------------------------------------------------- */

int empty_accumulator (trigram_t *tr, int ntr, trgstat_t **trgstat, int *ntrgstat)
{
    int i, j, nts1;
    trgstat_t *ts1;

    warning1 (".");
   /* warning1 ("going to merge %d items into accumulator containing %d items\n", */
   /*          ntr, *ntrgstat); */
    /* sort trigrams */
    qsort (tr, ntr, sizeof(trigram_t), cmp_integers);
    /* count unique trigrams */
    nts1 = 0;
    for (i=0; i<ntr; )
    {
        for (j=i; j<ntr; j++)
            if (tr[i] != tr[j]) break;
        nts1++;
        i = j;
    }

    /* make a trigram frequency table */
    ts1 = xmalloc (sizeof(trgstat_t) * nts1);
    nts1 = 0;
    for (i=0; i<ntr; )
    {
        for (j=i; j<ntr; j++)
            if (tr[i] != tr[j]) break;
        ts1[nts1].trigram = tr[i];
        ts1[nts1].n = j-i;
        nts1++;
        i = j;
    }
   /* warning1 ("%d entries in trigram frequency table\n", nts1); */

    /* merge frequency tables */
    merge_frequency_tables (trgstat, ntrgstat, ts1, nts1);

   /* warning1 ("%d items after merge\n", *ntrgstat); */
    free (ts1);
    return 0;
}

/* ---------------------------------------------------------------------- */

int merge_frequency_tables (trgstat_t **trgstat, int *ntrgstat,
                            trgstat_t *ts2, int nts2)
{
    int       i1, i2, nts, nts1;
    trgstat_t *ts1, *ts;

    /* allocate a lot of space (make sure both tables will fit at once */
    ts = xmalloc (sizeof (trgstat_t) * (*ntrgstat + nts2));

    /* scan sorted input tables and merge them */
    ts1 = *trgstat;
    nts1 = *ntrgstat;
   /* warning1 ("merging %d and %d items\n", nts1, nts2); */
    for (i1=0, i2=0, nts=0; i1<=nts1 && i2<=nts2; /* will be incremented inside */)
    {
        /* we have several cases: */
        if (i1 == nts1 && i2 == nts2)
        {
            /* both sources are empty */
            break;
        }
        else if (i1 == nts1)
        {
            /* source 1 is empty */
            ts[nts++] = ts2[i2++];
        }
        else if (i2 == nts2)
        {
            /* source 2 is empty */
            ts[nts++] = ts1[i1++];
        }
        else if (ts1[i1].trigram > ts2[i2].trigram)
        {
            /* source 1 > source 2. put element from source 2 into output */
            ts[nts++] = ts2[i2++];
        }
        else if (ts1[i1].trigram < ts2[i2].trigram)
        {
            /* source 1 < source 2. put element from source 1 into output */
            ts[nts++] = ts1[i1++];
        }
        else if (ts1[i1].trigram == ts2[i2].trigram)
        {
            /* source 1 == source 2. merge both sources */
            ts[nts].trigram = ts1[i1].trigram;
            ts[nts++].n = ts1[i1++].n + ts2[i2++].n;
        }
        else
        {
            error1 ("unexpected!\n");
        }
    }
   /* warning1 ("merged to %d\n", nts); */

    free (*trgstat);
    *trgstat = ts;
    *ntrgstat = nts;

    return 0;
}
