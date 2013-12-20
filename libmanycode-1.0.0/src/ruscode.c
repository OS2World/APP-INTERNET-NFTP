#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <asvtools.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charsets.h"
#include "manycode.h"

#define USE_MGUESSER 0

#define NOT_RUSSIAN 0xFF

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* possible values for guesser type:
 0 - pure rcode,
 1 - pure xcode,
 2 - utf8 + rcode + xcode,
 3 - asv trigram-based guesser,
 4 - mguesser (requires language maps at /usr/local/share/mguesser/maps)
 5 - asv trigram-based guesser + pure rcode
 */
int guesser_type = 5, guesser_babble = FALSE;

/* ------------------------------------------------------------------- */

typedef struct
 {
     BYTE   *pCharset;
     BYTE   Cpp;
     BYTE   Cp;
     long   lRusChars;
     long   lDecor;
     long   lRusPairs;
     long   lRarePairs;
     BYTE   To32[256];
 } tStream;

/* ------------------------------------------------------------------- */

int trigram_guess_cp (char *s);
int  UdmGuessCharSet(char *buf, char **charset);
int mguesser (char *buf);

static void MakeTable_To32 (BYTE *pCharset128, BYTE *pTable256);
static int GetGuess (tStream *streams);
static void AnalyseChar (BYTE bCh, tStream *streams);

/* ------------------------------------------------------------------- */

/* returns guessed encoding of string 's' */
int rcd_guess (char *s)
{
    tStream  streams[N_CHARSETS];
    int      i;

    for (i=0; i<N_CHARSETS; i++)
    {
        streams[i].pCharset = charsets[i].table;

        streams[i].Cpp = NOT_RUSSIAN;
        streams[i].Cp  = NOT_RUSSIAN;

        streams[i].lRusChars = 0;
        streams[i].lDecor = 0;

        streams[i].lRusPairs = 0;
        streams[i].lRarePairs = 0;

        MakeTable_To32 (charsets[i].table, streams[i].To32);
    }

    for (i=0; s[i] != '\0'; i++)
    {
        AnalyseChar (s[i], streams);
    }

    return GetGuess (streams);
}

/*------------------------------------------------------------------------*/

static void MakeTable_To32 (BYTE *pCharset128, BYTE *pTable256)
{
    int k;

    for( k=0; k<256; k++ )
        pTable256[k] = 0xFF;

    for( k=0; k<128; k++ )
    {
        BYTE C = pCharset128[k];

        /*--- Preprocess YO ---*/

        if (C == CapYo) C = CapE;
        else
            if (C == SmallYo) C = SmallE;

        /*--- Move:  A,a -> 0, ... , Ya,ya -> 31  ---*/

        if( (C>=CapA)&&(C<=CapYa) )
            pTable256[128+k] = C - CapA;
        else
            if( (C>=SmallA)&&(C<=SmallYa) )
                pTable256[128+k] = C - SmallA;
    }
}

/*------------------------------------------------------------------------*/

static int GetGuess (tStream *streams)
{
    int k;
    int  nBestCode = -1;
    long lMaxRusPairs = 0;
    long lC, lD, lP, lR;

    for (k=0; k<N_CHARSETS; k++)
    {
        lC = streams[k].lRusChars;
        lD = streams[k].lDecor;
        lP = streams[k].lRusPairs;
        lR = streams[k].lRarePairs;
        if (lP)
        {
            if (lR < 0.3*lP)
            {
                if (lD < 0.7*lC)
                {
                    if (lP > lMaxRusPairs)
                    {
                        lMaxRusPairs = lP;
                        nBestCode = k;
                    }
                }
            }
        }
    }

    if (nBestCode == -1)
    {
        for( k=0; k<N_CHARSETS; k++ )
        {
            lC = streams[k].lRusChars;
            lD = streams[k].lDecor;
            lP = streams[k].lRusPairs;
            if (lP)
            {
                if (lD < 0.7*lC)
                {
                    if (lP > lMaxRusPairs)
                    {
                        lMaxRusPairs = lP;
                        nBestCode = k;
                    }
                }
            }
        }
    }

    if (nBestCode == -1) return EN_UNKNOWN;
    return charsets[nBestCode].cn;
}

/*------------------------------------------------------------------------*/

static void AnalyseChar (BYTE bCh, tStream *streams)
{
    int  k;
    BYTE C;
    
    for (k=0; k<N_CHARSETS; k++)
    {
        C = streams[k].To32[bCh];
    
        if (streams[k].Cp != NOT_RUSSIAN)
        {
            streams[k].lRusChars++;
    
            if (C != NOT_RUSSIAN)
            {
                streams[k].lRusPairs++;
                if (C == streams[k].Cp)
                    streams[k].lDecor++;
                if (RarePairsTable[streams[k].Cp][C])
                    streams[k].lRarePairs++;
            }
            else
            {
                if (streams[k].Cpp == NOT_RUSSIAN)
                {
                    streams[k].lDecor++;
                }
            }
        }
    
        streams[k].Cpp = streams[k].Cp;
        streams[k].Cp = C;
    }
}

/* translates string 's' in place from encoding 'from' to 'to'.
 'from' could be EN_UNKNOWN (the guess will be used). returns 0
 if success, and -1 in case of error (unable to guess, or wrong
 parameters). on error 's' is unmodified */
int rcd_translate (char *s, int from, int to)
{
    char *s1, *s2;
    
   /* verify parameters */
   /* reject NULL instead of string */
    if (s == NULL) return -1;
   /* if string is empty, return OK */
    if (s[0] == '\0') return 0;
   /* cannot convert to unknown charset */
    if (to == EN_UNKNOWN) return -1;

    if (from == EN_UNKNOWN)
    {
        from = rcd_guess (s);
        if (from == EN_UNKNOWN) return -1;
    }

    s1 = cp2utf (s, from);
    s2 = utf2cp (s1, to);
    strcpy (s, s2);
    free (s1);
    free (s2);

    return 0;
}

/* length limit for rcode algorithm. it is painfully slow on
 large strings; one megabyte could take up to three hours... */
#define RCODE_LENGTH_LIMIT 1024
/* #define RCODE_LENGTH_LIMIT 128 */
#define XCODE_LENGTH_LIMIT 512
/* #define XCODE_LENGTH_LIMIT 32 */
#define UTF8_LENGTH_LIMIT  512
/* #define UTF8_LENGTH_LIMIT  0 */

enum {IN_ENG, IN_RUS};

int rus_encoding (char *s)
{
    char buf[RCODE_LENGTH_LIMIT+2];
    int  i, n, state, enc, c1, c2, cp;

    /* we do the following: copy bytes one-by-one from 's' to 'buf'.
     only high-half ascii table bytes are copied. when non-high byte
     is detected space is copied. */

    state = IN_ENG;

    for (i=0, n=0; s[i] != '\0'; i++)
    {
        if (n == RCODE_LENGTH_LIMIT) break;

        switch (state)
        {
        case IN_ENG:
            if ((unsigned char)(s[i]) < 127) break;
            buf[n++] = s[i];
            state = IN_RUS;
            break;

        case IN_RUS:
            if ((unsigned char)(s[i]) < 127)
            {
                buf[n++] = ' ';
                state = IN_ENG;
            }
            else
            {
                buf[n++] = s[i];
            }
            break;
        }
    }
    buf[n] = '\0';

   /* printf ("source:[%s], buffer:[%s], %d bytes\n", s, buf, n); */
    if (n == 0) return EN_UNKNOWN;

    /* first we check if this is UTF8. check is fairly simple and
     only works for Russian texts */
    if (guesser_type == 2 && n >= UTF8_LENGTH_LIMIT)
    {
        c1 = 0, c2 = 0;
        for (i=0; i<n; i++)
        {
            if (buf[i] == 'Ð') c1++;
            else if (buf[i] == 'Ñ') c2++;
        }
       /* printf ("%f, %f\n", (double)c1/(double)n, (double)c2/(double)n); */
        if ((double)c1/(double)n > 0.2) return EN_UTF8;
    }

    switch (guesser_type)
    {
    case 0:
        return rcd_guess (buf);

    case 1:
        return rcd_guess_xcode (buf);

    case 2:
        enc = rcd_guess (buf);
        if (enc != EN_UNKNOWN) return enc;
        if (n < XCODE_LENGTH_LIMIT) return EN_UNKNOWN;
        return rcd_guess_xcode (buf);

    case 3:
        return trigram_guess_cp (buf);

    case 4:
        return mguesser (buf);

    case 5:
        cp = trigram_guess_cp (buf);
        if (cp == EN_UNKNOWN) cp = rcd_guess (buf);
        return cp;
    }

    return EN_UNKNOWN;
}

/* replaces 'yo' to 'ye' in cyrillic KOI-8R texts. when 'len=0',
 string is considered NUL-terminated */
#define YO 179 /* ³ */
#define yo 163 /* £ */
#define YE 229 /* å */
#define ye 197 /* Å */
int replace_yo (char *s, int len)
{
    int  i;
    unsigned char *us;

    us = (unsigned char *)s;
    if (len == 0)
    {
        while (*us)
        {
            if (*us == YO)      *us = YE;
            else if (*us == yo) *us = ye;
            us++;
        }
    }
    else
    {
        for (i=0; i<len; i++)
        {
            if (us[i] == YO)      us[i] = YE;
            else if (us[i] == yo) us[i] = ye;
        }
    }
    return 0;
}

#define NUMCOD 5

/* char * encName[]={"koi8","cp866","cp1251","iso8859-5","mac"}; */
static int enc_code[] = {EN_KOI8R, EN_ALT, EN_WINDOWS, EN_ISO, EN_MAC};

static unsigned char recode_table[NUMCOD][128] =
{
    { /* koi8 */
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
        144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,'-',174,175,
        176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
        208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
        240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    },
    {/* dos */
        225,226,247,231,228,229,246,250,233,234,235,236,237,238,239,240,
        242,243,244,245,230,232,227,254,251,253,255,249,248,252,224,241,
        193,194,215,199,196,197,214,218,201,202,203,204,205,206,207,208,
        128,129,130,131,132,133,134,135,136,137,186,139,140,141,142,143,
        144,145,146,147,148,149,150,151,152,153,154,191,156,157,158,159,
        160,161,162,176,164,165,166,167,168,169,170,171,172,173,174,175,
        210,211,212,213,198,200,195,222,219,221,223,217,216,220,192,209,
        179,163,'+','+','+','+','+','+',184,'+','+','+','+','+',190,' '
    },
    {/* win */
        '+','+', 39,'+', 34,'+','+','+','+','+','+', 39,'+','+','+','+', '+', 39, 39, 34,
        34,'+','+','-','-','*','+', 39,'+','+','+','+', ' ','+','I','+','+','+','+','+',179,188,'E',
        34,'+','+','*','I', 184,'+','i',199,'*','*','*','*',163,'N','e', 34,'j','S','s','i',
        225,226,247,231,228,229,246,250,233,234,235,236,237,238,239,240,
        242,243,244,245,230,232,227,254,251,253,255,249,248,252,224,241,
        193,194,215,199,196,197,214,218,201,202,203,204,205,206,207,208,
        210,211,212,213,198,200,195,222,219,221,223,217,216,220,192,209
    },
    {/* iso */
        '+','+','+','+','+','+','+','+','+','+','+','+','+','+','+','+',
        '+','+','+','+','+','+','+','+','+','+','+','+','+','+','+','+',
        '*',179,'*','*','*','*','*','*','*','*','*','*','*','*','*','*',
        225,226,247,231,228,229,246,250,233,234,235,236,237,238,239,240,
        242,243,244,245,230,232,227,254,251,253,255,249,248,252,224,241,
        193,194,215,199,196,197,214,218,201,202,203,204,205,206,207,208,
        210,211,212,213,198,200,195,222,219,221,223,217,216,220,192,209,
        '*',163,'*','*','*','*','*','*','*','*','*','*','*','*','*','*'
    },
    {/* mac */
        225,226,247,231,228,229,246,250,233,234,235,236,237,238,239,240,
        242,243,244,245,230,232,227,254,251,253,255,249,248,252,224,241,
        '*',184,'*','*','*','*','*','I','*','*','*','*','*','*','*','*',
        '*',179,177,178,'i','*',199,'J','E','e','I','i','*','*','*','*',
        'j','S','*','*','f','*','*','*','*','*','*','*','*','*','*','s', '-','-', 34, 34, 39, 39,'*',
        39,'*','*','*','*','N',163,179,209,
        193,194,215,199,196,197,214,218,201,202,203,204,205,206,207,208,
        210,211,212,213,198,200,195,222,219,221,223,217,216,220,192,'*'
    }
};

static int letter_frequency[32]=
{
    125,1606,331,65,598,1714,22,356,168,1312,206,636,1050,658,1295,2259,
    544,447,887,1049,1217,572,200,823,398,400,355,168,67,78,285,4
};

int rcd_guess_xcode (char *s)
{
    int     i, j, a=0;
    double  rating[NUMCOD], maxrating=-1.0;
    unsigned char *us;

    memset(rating, 0 , sizeof(double) * NUMCOD); 

    us = (unsigned char *)s;
    for (i=0; s[i] != '\0'; i++)
    {
        if(us[i]>=128)
            for(j=0; j<NUMCOD; j++)
                if(recode_table[j][us[i]-128]>=192)
                    rating[j] += letter_frequency[((int)recode_table[j][us[i]-128]-192)%32];
    }

    for(i=0; i<NUMCOD; i++) 
	if ( rating[i] > maxrating ) {
		a = i;
		maxrating = rating[i];
	} 

    return enc_code[a];
}

/* convert text to KOI8 and return codepage which was before converting */
int convert_to_koi8 (char *s)
{
    int cp;

    cp = rus_encoding (s);

    /* if codepage is Russian but not KOI8 convert into KOI8 */
    if (cp == EN_ALT || cp == EN_MAC || cp == EN_ISO || cp == EN_WINDOWS || cp == EN_UTF8)
    {
        ru_convert (s, cp);
    }
    if (cp == EN_KOI8R || cp == EN_ALT || cp == EN_MAC || cp == EN_ISO || cp == EN_WINDOWS || cp == EN_UTF8)
    {
        replace_yo (s, 0);
    }

    return cp;
}

/* -------------- trigram-based guessing -------------------------------- */

typedef int trigram_t;
typedef struct
{
    int trigram;
    int weight;
}
trigramweight_t;

static struct
{
    int cp;
    int weight;
}
encodings[] =
{
    {EN_ALT,     0},
    {EN_KOI8R,   0},
    {EN_ISO    , 0},
    {EN_WINDOWS, 0},
    /* Mac must follow Windows. if they have equal weights Windows will have
     precedence then */
    /*{EN_MAC,     0}, don't try to guess Mac encoding at all. it seems to
     be unused in Russia */
    {EN_UTF8,    0}
};

#define n_encodings (sizeof(encodings)/sizeof(encodings[0]))

static trigramweight_t trigramweight[] =
{
#include "trigrams.kov"
/* #include "trigrams.libru" */
};

static int missing_penalty = 0;

/* ---------------------------------------------------------------------- */

int compute_trigrams (char *s, trigram_t *trg);
static int cmp_trigrams (const void *e1, const void *e2);

/* ---------------------------------------------------------------------- */
#define TRIGRAM_LENGTH_LIMIT 512

int trigram_guess_cp (char *s)
{
    int  i, j, k, ln, ntrg, nw, enc, max_weight, misses;
    char            *s1, *s2=NULL, **w, *p;
    trigram_t       trg[34];
    trigramweight_t *ptw, tw1;

    if (missing_penalty == 0)
    {
        for (i=0; i<sizeof(trigramweight)/sizeof(trigramweight[0]); i++)
        {
            missing_penalty = max1 (missing_penalty, trigramweight[i].weight);
        }
        /* missing_penalty /= 2; */
    }

    ln = min1 (TRIGRAM_LENGTH_LIMIT, strlen (s) + 1);
    s1 = xmalloc (ln);
    /* cycle by all known encodings and compute their weights */
    for (i=0; i<n_encodings; i++)
    {
        if (guesser_babble) printf ("%s:\n", ru_cp_name(encodings[i].cp));
        encodings[i].weight = 0;

        /* make a copy of the string */
        memcpy (s1, s, ln);
        s1[ln-1] = '\0';

        /* suppose it is in encodings[i].cp. convert to KOI8 and lowercase */
        if (encodings[i].cp == EN_UTF8)
        {
            s2 = utf2cp (s1, EN_KOI8R);
            p = s2;
        }
        else
        {
            ru_convert (s1, encodings[i].cp);
            p = s1;
        }
        strlwr (p);
        replace_yo (p, 0);

        /* break into letter-only words */
        nw = str_words (p, &w, WSEP_LETTERS);
        if (nw == 0)
        {
            if (encodings[i].cp == EN_UTF8) free (s2);
            continue;
        }

        /* for every word... */
        for (j=0; j<nw; j++)
        {
            /* ignore single letters and too long garbage */
            if (strlen (w[j]) < 2) continue;
            if (strlen (w[j]) > 32) continue;

            /* compute trigrams for this word */
            ntrg = compute_trigrams (w[j], trg);

            if (guesser_babble) printf ("  %s: ", w[j]);
            misses = 0;
            /* locate every trigram in trigramweight array and add corresponding
             weight to encoding being tested */
            for (k=0; k<ntrg; k++)
            {
                tw1.trigram = trg[k];
                ptw = bsearch (&tw1, trigramweight,
                               sizeof(trigramweight)/sizeof(trigramweight[0]),
                               sizeof(trigramweight[0]), cmp_trigrams);
                if (ptw != NULL)
                {
                    encodings[i].weight += ptw->weight;
                    if (guesser_babble)
                        printf ("%d/%d ", trg[k], ptw->weight);
                }
                else
                {
                    misses++;
                }
            }

            /* apply penalties */
            if (misses > 0)
                encodings[i].weight -= missing_penalty * misses;
            if (guesser_babble) printf ("\n");
        }

        if (encodings[i].cp == EN_UTF8) free (s2);
        free (w);
    }
    free (s1);

    enc = encodings[0].cp;
    max_weight = encodings[0].weight;
    if (guesser_babble)
        printf ("%s -> %d\n", ru_cp_name(encodings[0].cp),
                encodings[0].weight);
    for (i=1; i<n_encodings; i++)
    {
        if (guesser_babble)
            printf ("%s -> %d\n", ru_cp_name(encodings[i].cp),
                    encodings[i].weight);
        if (encodings[i].weight > max_weight)
        {
            enc = encodings[i].cp;
            max_weight = encodings[i].weight;
        }
    }

    if (max_weight < 2) return EN_UNKNOWN;
    return enc;
}

/* ---------------------------------------------------------------------- */

int compute_trigrams (char *s, trigram_t *trg)
{
    int           i, ntrg1;
    unsigned char *s1;

    s1 = (unsigned char *)s;
    ntrg1 = strlen (s);

    trg[0] = s1[0]*256 + s1[1];
    for (i=1; i<ntrg1-1; i++)
    {
        trg[i] = s1[i-1]*256*256 + s1[i]*256 + s1[i+1];
    }
    trg[ntrg1-1] = s1[ntrg1-1-1]*256*256 + s1[ntrg1-1]*256;

    return ntrg1;
}

/* ---------------------------------------------------------------------- */

static int cmp_trigrams (const void *e1, const void *e2)
{
    trigramweight_t *tw1, *tw2;

    tw1 = (trigramweight_t *) e1;
    tw2 = (trigramweight_t *) e2;

    return tw1->trigram - tw2->trigram;
}

/* ---------------------------------------------------------------------- */

int mguesser (char *buf)
{
#if USE_MGUESSER
    char *charset;
    int  enc;

    if (guesser_babble) printf ("Guessing '%s'\n", buf);
    UdmGuessCharSet (buf, &charset);

    if (charset == NULL) return EN_UNKNOWN;

    if      (strcmp (charset, "cp1251")       == 0) enc = EN_WINDOWS;
    else if (strcmp (charset, "cp866")        == 0) enc = EN_ALT;
    else if (strcmp (charset, "iso-8859-5")   == 0) enc = EN_ISO;
    else if (strcmp (charset, "koi8-r")       == 0) enc = EN_KOI8R;
    else if (strcmp (charset, "maccyrillic")  == 0) enc = EN_MAC;
    else enc = EN_UNKNOWN;
    free (charset);

    return enc;
#else
    return EN_UNKNOWN;
#endif
}

#if USE_MGUESSER
#include "mguesser.c"
#endif

