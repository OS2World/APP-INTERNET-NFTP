#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <errno.h>

#include <asvtools.h>
#include <manycode.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

enum {MODE_CONVERT, MODE_TEST, MODE_GUESS, MODE_BENCHMARK};

static int enc[] = {EN_ALT, EN_KOI8R, EN_ISO, EN_WINDOWS, EN_UTF8, EN_MAC};
static int n_enc = sizeof (enc) / sizeof(int);
static int *enc_err;
static int mode, byline, ignore_mac, print_unknown, print_known;

/*********************************************************************/

static int what_charset (char *s);
static int process_text (char *buf, int from, int to);
static int print_charsets (void);

/*********************************************************************/

int usage (int rc, char *errmsg)
{
    if (errmsg != NULL) fprintf (stderr, "%s\n", errmsg);

    fprintf (stderr,
             "\n"
             "This program will read standard input and translate it\n"
             "from one charset to another. Translated text goes to \n"
             "standard output.\n"
             "\n"
             "Usage:\n"
             "  ruscode [-bklmuBGT] [-g guesser] [-f filename] [[charset-from] charset-to]\n"
             "\n"
             "When charset-from is not specified it will be autodetected.\n"
             "When charset-to is not specified it is assumed to be KOI8-R.\n"
             "Valid cyrillic charsets and synonyms:\n"
             "  866  a - alternative (DOS, OS/2)\n"
             "  878  k - KOI8-R (Unix)\n"
             "  915  i - ISO 8859-5\n"
             "  1251 w - Windows\n"
             "  1283 m - Mac cyrillic\n"
             "  1208 u - UTF-8\n"
             "\n"
             "Options:\n"
             "  -f filename: take input from 'filename' instead of standard\n"
             "     input\n"
             "  -l: line mode (process every line of input individually,\n"
             "     including charset guessing). Line length must not\n"
             "     exceed 8192 symbols\n"
             "  -g guesser: select cyrillic charset guesser:\n"
             "     0 - rcode guesser\n"
             "     1 - xcode guesser\n"
             "     2 - utf8 guesser + rcode guesser + xcode guesser\n"
             "     3 - trigram-based guesser (default)\n"
             "     4 - mguesser (NOT AVAILABLE)\n"
             "  -b: print guesser debug information\n"
             "  -k: in line mode, output only lines with successfully\n"
             "     guessed charset\n"
             "  -u: in line mode, output only lines with guess failure\n"
             "\n"
             "Special modes of operation:\n"
             "  -B: run guess benchmark on input text\n"
             "  -G: guess and print guessed charset. In line mode text is\n"
             "     also printed\n"
             "  -T: run guessing tests on input and print statistics.\n"
             "     Input MUST be in KOI8-R\n"
             "  -m: in testing mode (-T) consider Windows and Mac charsets\n"
             "     identical\n"
             "  -E: print list of supported charsets\n"
             "\n"
             "Examples:\n"
             "  guess input charset, convert to KOI8-R:\n"
             "    ruscode k\n"
             "  convert from DOS to Windows charset:\n"
             "    ruscode 866 w\n"
             "  guess charset of the input text:\n"
             "    ruscode -G\n"
             "  guess input charset of every line and print it:\n"
             "    ruscode -l -G\n"
             "  run tests on guesser 3 for every line of input:\n"
             "    ruscode -l -g3 -T\n"
             "  benchmark guesser 4 using input:\n"
             "    ruscode -g4 -B\n"
             "\n"
           );

    if (rc) exit (rc);
    return 0;
}

/*********************************************************************/

int main (int argc, char *argv[])
{
    int   i, from, to, nb, c, nl;
    char  buf[8192+3], *p, *progname, *filename, *locale;
    double t1;
    FILE  *fp;

    byline = FALSE;
    ignore_mac = FALSE;
    print_unknown = FALSE;
    print_known = FALSE;
    mode = MODE_CONVERT;
    filename = NULL;
    locale = "ru_RU.KOI8-R";

    progname = strdup (argv[0]);
    optind  = 1;
    opterr  = 0;
    while ((c = getopt (argc, argv, "be:f:g:klmuBEGLT")) != -1)
    {
        switch (c)
        {
        case 'b':
            guesser_babble = TRUE;
            break;

        case 'e':
            locale = strdup (optarg);
            break;

        case 'f':
            filename = strdup (optarg);
            break;

        case 'g':
            guesser_type = atoi (optarg);
            break;

        case 'k':
            print_known = TRUE;
            break;

        case 'l':
            byline = TRUE;
            break;

        case 'm':
            ignore_mac = TRUE;
            break;

        case 'u':
            print_unknown = TRUE;
            break;

        case 'B':
            mode = MODE_BENCHMARK;
            break;

        case 'E':
            print_charsets ();
            return 0;

        case 'G':
            mode = MODE_GUESS;
            break;

        case 'L':
            locale = "";
            break;

        case 'T':
            mode = MODE_TEST;
            break;

        default:
            usage (0, NULL);
            return 0;
        }
    }
    argv += optind;
    argc -= optind;

    if (setlocale (LC_CTYPE, locale) == NULL ||
        setlocale (LC_COLLATE, locale) == NULL)
        error1 ("Failed to set locale to '%s'\n", locale);

    /* interpret charset arguments */
    from = EN_UNKNOWN;
    to   = EN_UNKNOWN;
    if (argc < 0 || argc > 2) usage (-1, "Incorrect number of arguments");
    if (argc >  1) to = what_charset (argv[1]), from = what_charset (argv[0]);
    if (argc == 1) to = what_charset (argv[0]);

    /* in GUESS, TEST and BENCHMARK modes input/output charsets
     are unnecessary and must be omitted */
    switch (mode)
    {
    case MODE_TEST:
        /* ignore Mac if specified */
        if (ignore_mac) n_enc--;
        /* fall through */

    case MODE_GUESS:
    case MODE_BENCHMARK:
        if (to != EN_UNKNOWN || from != EN_UNKNOWN)
            usage (-1, "To/from charsets are not used in this mode\n");
        break;

    case MODE_CONVERT:
        if (to == EN_UNKNOWN) to = EN_KOI8R;
        if (to == from) usage (-1, "charset-to and charset-from must differ");
    }

    /* some modes require initializations */
    switch (mode)
    {
    case MODE_TEST:
        enc_err = xmalloc (sizeof (int) * n_enc);
        memset (enc_err, 0, sizeof (int) * n_enc);
    }

    /* main processing */
    nl = 0;
    t1 = clock1 ();
    if (byline)
    {
        /* open input if required */
        if (filename != NULL)
        {
            fp = fopen (filename, "r");
            if (fp == NULL)
                error1 ("cannot open %s (%s)\n", filename, strerror (errno));
        }
        else
        {
            fp = stdin;
        }

        /* read line-by-line */
        while (fgets (buf, sizeof(buf), fp) != NULL)
        {
            nl++;
            str_strip (buf, "\r\n");
            if (process_text (buf, from, to)) printf ("\n");
        }

        if (filename != NULL)
            fclose (fp);
    }
    else
    {
        nl++;
        if (filename == NULL)
        {
            nb = load_stdin (&p);
            if (nb < 0)
                error1 ("error reading standard input (%s)\n", strerror (errno));
        }
        else
        {
            nb = load_file (filename, &p);
            if (nb < 0)
                error1 ("error reading %s (%s)\n", filename, strerror (errno));
        }
        process_text (p, from, to);
        free (p);
    }

    switch (mode)
    {
    case MODE_TEST:
        if (nl > 0)
        {
            warning1 ("%d words tested in %.3f sec;\n"
                      "charset detection failure statistics:\n", nl, clock1() - t1);
            for (i=0; i<n_enc; i++)
            {
                warning1 ("%14s: %5d failures (%.2f%%)\n", ru_cp_name(enc[i]),
                          enc_err[i], 100.0*(double)enc_err[i]/(double)nl);
            }
        }
        break;
    }

    return 0;
}

/***********************************************************************/

static int what_charset (char *s)
{
    if (isdigit ((unsigned char)s[0]))
    {
        return atoi (s);
    }
    else
    {
        switch (s[0])
        {
        case 'a': return EN_ALT; break;
        case 'k': return EN_KOI8R; break;
        case 'i': return EN_ISO; break;
        case 'w': return EN_WINDOWS; break;
        case 'm': return EN_MAC; break;
        case 'u': return EN_UTF8; break;
        }
        return EN_UNKNOWN;
    }
}

/***********************************************************************/

static int process_text (char *buf, int from, int to)
{
    char *p, *p1, *ut=NULL;
    int  from1, count;
    int i, g_enc, errcnt = 0;
    double t1;

    switch (mode)
    {
    case MODE_GUESS:
        if (byline)
            printf ("%s: %s\n", buf, ru_cp_name (rus_encoding (buf)));
        else
            printf ("%s\n", ru_cp_name (rus_encoding (buf)));
        return 0;

    case MODE_TEST:
        /* in this mode input is always in KOI8 */
        strlwr (buf);
       /* printf ("%s: ", buf); */
        for (i=0; i<n_enc; i++)
        {
            /* for every encoding we do the following: */
           /* printf ("[%s: %s -> ", ru_cp_name(enc[i]), buf); */

            /* 1. convert to this encoding (if it is not koi8) */
            if (enc[i] != EN_KOI8R)
            {
                if (enc[i] == EN_UTF8)
                {
                    ut = cp2utf (buf, EN_KOI8R);
                    p = ut;
                }
                else
                {
                    ru_convert2 (buf, enc[i]);
                    p = buf;
                }
            }
            else
                break;
           /* printf ("%s, ", buf); */

            /* 2. try to autodetect it */
            g_enc = rus_encoding (p);
            /* 2a. ignore Mac encoding */
            if (ignore_mac && g_enc == EN_MAC) g_enc = EN_WINDOWS;
           /* printf ("detected %s", ru_cp_name(g_enc)); */

            /* 3. convert back */
            if (enc[i] != EN_KOI8R)
            {
                if (enc[i] == EN_UTF8)
                {
                    free (ut);
                }
                else
                {
                    ru_convert (buf, enc[i]);
                }
            }

            /* 4. print negative results */
            if (g_enc != enc[i])
            {
               /* printf ("%s error (%s -> %s), ", */
               /*        ru_cp_name(enc[i]), p, ru_cp_name(g_enc)); */
                enc_err[i]++;
                errcnt++;
            }
           /* if (print_unknown && g_enc == EN_UNKNOWN) */
           /* { */
           /*    printf ("%s\n", buf); */
           /* } */
           /* printf ("] "); */
        }
       /* if (errcnt > 0) printf ("%d errors\n", errcnt); */
       /* else            printf ("OK\n"); */
        return 0;

    case MODE_CONVERT:
        /* charset guessing if necessary */
        if (from == EN_UNKNOWN) from1 = rus_encoding (buf);
        else                    from1 = from;

        if (from1 == EN_UNKNOWN)
        {
            /* guess failure */
            if (!(byline && print_known))
            {
                printf ("%s", buf);
                return 1;
            }
            else
                return 0;
        }
        else
        {
            if (!(byline && print_unknown))
            {
                /* guess success or explicit conversion */
                if (to == EN_KOI8R && from1 != EN_UTF8)
                {
                    ru_convert (buf, from1);
                    printf ("%s", buf);
                }
                else if (from1 == EN_KOI8R && to != EN_UTF8)
                {
                    ru_convert2 (buf, to);
                    printf ("%s", buf);
                }
                else if (from1 == EN_UTF8)
                {
                    p = utf2cp (buf, to);
                    printf ("%s", p);
                    free (p);
                }
                else if (to == EN_UTF8)
                {
                    p = cp2utf (buf, from1);
                    printf ("%s", p);
                    free (p);
                }
                else
                {
                    p1 = cp2utf (buf, from1);
                    p = utf2cp (p1, to);
                    free (p1);
                    printf ("%s", p);
                    free (p);
                }
                return 1;
            }
            else
                return 0;
        }
        break;

    case MODE_BENCHMARK:
        count = 10;
        while (TRUE)
        {
            t1 = clock1 ();
            for (i=0; i<count; i++)
            {
                rus_encoding (buf);
            }
            t1 = clock1 () - t1;
            if (t1 > 1.0) break;
            count *= 10;
        }
        warning1 ("%.3f msec per single guess\n", t1/(double)count*1000.0);
        return 0;
    }

    return 0;
}

/***********************************************************************/

static int print_charsets (void)
{
    printf ("Supported charsets:\n"
            "\n"
            "  437 - PC United States (includes pseudographics)\n"
            "  813 - Greek ISO 8859-7\n"
            "  850 - Multilingual\n"
            "  852 - Latin 2\n"
            "  855 - Cyrillic\n"
            "  857 - Latin 5\n"
            "  860 - Portugal\n"
            "  861 - Iceland\n"
            "  862 - Israel\n"
            "  863 - Canadian (French)\n"
            "  864 - Arabic\n"
            "  865 - Nordic\n"
            "  866 - Russian cyrillic alternative\n"
            "  869 - Greece\n"
            "  878 - Russian cyrillic KOI8-R\n"
            "  912 - Latin 2 ISO 8859-2\n"
            "  913 - Latin 3 ISO 8859-3\n"
            "  914 - Latin 4 ISO 8859-4\n"
            "  915 - Russian cyrillic ISO 8859-5\n"
            "  916 - Israel ISO 8859-8\n"
            "  920 - Latin 5 ISO 8859-9\n"
            " 1208 - UTF8 (this is not a charset actually)\n"
            " 1250 - Latin 2 Windows\n"
            " 1251 - Cyrillic Windows\n"
            " 1252 - Latin 1 Windows\n"
            " 1253 - Greece Windows\n"
            " 1254 - Turkey Windows\n"
            " 1255 - Israel Windows\n"
            " 1256 - Arabic Windows\n"
            " 1257 - Baltic Windows\n"
            " 1275 - Apple Latin 1\n"
            " 1280 - Apple Greece\n"
            " 1282 - Apple Central Europe\n"
            " 1283 - Cyrillic Macintosh\n"
            "\n"
            "Source: IBM OS/2 Warp 4. Keyboards and Code Pages. 1996, 29H3183\n"
           );
    return 0;
}
