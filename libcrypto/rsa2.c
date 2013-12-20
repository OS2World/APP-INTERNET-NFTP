#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "bn.h"
#include "scl.h"
#include <asvtools.h>

char *rsa_encode (char *s, int length, char *key)
{
    // we use NULL-with-random padding. this allows to pass
    // NULL-terminated strings without any additional processing.
    // if you want to exchange binary data, define your own padding in
    // your application or pass the data length somehow

    BIGNUM         from, to, *n=NULL, *e=NULL;
    int            i, nl, nc, nb, nb_a, pc, rc1, rc2, len;
    unsigned char  *buf = NULL;
    BN_CTX         *ctx = NULL;
    char           *p, *p1, *output;

    // setup key (n, de)
    p = strdup (key);
    p1 = strchr (p, ':');
    if (p1 == NULL) {free (p); return NULL;}
    *p1 = '\0';
    rc1 = BN_hex2bn (&n, p);
    rc2 = BN_hex2bn (&e, p1+1);
    free (p);
    if (rc1 == 0 || rc2 == 0) return NULL;
                 
    // initialize temp variables
    BN_init (&from);
    BN_init (&to);
    ctx = BN_CTX_new ();
    if (ctx == NULL) return NULL;

    // number of bytes in the modulus. this is the amount of bytes
    // we can convert in one gulp
    nl = BN_num_bytes (n) - 1;
    buf = malloc (nl);
    if (buf == NULL) return NULL;

    // compute the 'nc', the number of cycles (gulps)
    nc = length/nl;
    if (length % nl) nc++;
    
    // preallocate output buffer: nl*2 -- bin->hex conversion,
    // nl*2+1 -- spaces between gulps in the output
    nb_a = nc * (nl*2 + 1) + 1;
    nb = 0;
    output = malloc (nb_a);
    if (output == NULL) return NULL;

    // cycle by pieces of input, each piece is 'nl' bytes long
    // (except the last one)
    for (i=0; i<nc; i++)
    {
        // compute piece length
        pc = (i == nc-1) ? length % nl : nl;
        memcpy (buf, s+i*nl, pc);
        // do random padding if necessary after first NULL
        if (pc != nl)
        {
            buf[pc] = '\0';
            if (nl-pc-1 > 0)
                rand_bytes (buf+pc+1, nl-pc-1);
            //memset (buf+pc, 0, nl-pc);
        }
        // convert to bignum
        BN_bin2bn (buf, nl, &from);
        // RSA
        BN_mod_exp (&to, &from, e, n, ctx);
        // convert into hex
        p = BN_bn2hex (&to);
        // copy result to output buffer and add delimiting space
        len = strlen (p);
        if (nb+len+1 >= nb_a)
        {
            nb_a *= 2;
            output = realloc (output, nb_a);
            if (output == NULL) return NULL;
        }
        memcpy (output+nb, p, len); nb += len;
        output[nb++] = ' ';
        free (p);
    }
    output[--nb] = '\0';
    
    BN_CTX_free (ctx);
    BN_clear_free (&from);
    BN_clear_free (&to);
    memset (buf, 0, nl);
    free(buf);

    return output;
}

int rsa_decode (char *s, char *key, char **result)
{
    // there is no any padding processing in the decoding routine
    //  (see comment in rsa_encode)

    BIGNUM         *from, to, *n=NULL, *d=NULL;
    int            i, nl, nc, rc1, rc2, length;
    unsigned char  *buf = NULL;
    BN_CTX         *ctx = NULL;
    char           *p, *p1, *p2;

    // setup key (n, de)
    p = strdup (key);
    p1 = strchr (p, ':');
    if (p1 == NULL) {free (p); return -1;}
    *p1 = '\0';
    rc1 = BN_hex2bn (&n, p);
    rc2 = BN_hex2bn (&d, p1+1);
    free (p);
    if (rc1 == 0 || rc2 == 0) return -1;
                 
    // initialize temp variables
    BN_init (&to);
    ctx = BN_CTX_new ();
    if (ctx == NULL) return -1;

    // number of bytes in the modulus. this is the amount of bytes
    // we can convert in one gulp and should expect to be in one
    // group
    nl = BN_num_bytes (n) - 1;
    buf = malloc (nl);
    if (buf == NULL) return -1;

    // find the number of pieces in the encrypted message (the last
    // piece is not terminated with space)
    nc = str_numchars (s, ' ') + 1;
    
    // preallocate output buffer
    length = nc * nl;
    *result = malloc (length);
    if (*result == NULL) return -1;

    // cycle by pieces of input, each piece is 'nl' bytes long
    // (except the last one)
    p1 = s;
    for (i=0; i<nc; i++)
    {
        // extract next piece
        p2 = strchr (p1, ' ');
        if (p2 == NULL)
        {
            if (i != nc-1) return -1;
        }
        else
        {
            *p2 = '\0';
        }
        // convert to bignum
        from = NULL;
        rc1 = BN_hex2bn (&from, p1);
        if (rc1 == 0) return -1;
        // RSA
        BN_mod_exp (&to, from, d, n, ctx);
        BN_clear_free (from);
        // convert into binary output
        BN_bn2bin (&to, (unsigned char *)(*result+i*nl));
        // advance pointer to prepare search for next piece
        p1 = p2 + 1;
    }
    
    BN_CTX_free (ctx);
    BN_clear_free (&to);
    memset (buf, 0, nl);
    free(buf);

    return length;
}

/* computes RSA signature for ASCIIZ message with private_key.
 returns malloc()ed buffer with hexadecimal representation of signature */
char *rsa_sign (char *message, int length, char *private_key)
{
    char hash[20];
    char *p;

    sha1 ((unsigned char *)message, length, (unsigned char *)hash);
    p = rsa_encode (hash, 20, private_key);

    return p;
}

/* verifies RSA signature computed by rsa_sign(). message is NULL-terminated
 string, signature is in hex representation (as computed by rsa_sign).
 returns 1 if verified, 0 if verification failed */
int rsa_verify (char *message, int length, char *signature, char *public_key)
{
    unsigned char   hash1[20];
    char   *hash2;
    int    hl, verified;

    verified = 1;
    sha1 ((unsigned char *)message, length, hash1);
    hl = rsa_decode (signature, public_key, &hash2);
    if (hl < 20)
    {
        verified = 0;
    }
    else
    {
        if (memcmp (hash1, hash2, 20) != 0) verified = 0;
    }
    free (hash2);
    
    return verified;
}

