/* crypto/rsa/rsa_gen.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <string.h>
#include <stdlib.h>
#include "bn.h"
#include "scl.h"

int rsa_keypair (int bits, unsigned long e_value, char **public_key, char **secret_key)
{
    BIGNUM  *n=NULL, *e=NULL, *d=NULL, *p=NULL, *q=NULL;
    BIGNUM  *r0=NULL, *r1=NULL, *r2=NULL, *r3=NULL, *tmp;
    int     bitsp, bitsq, i;
    BN_CTX  *ctx=NULL, *ctx2=NULL;
    char    *buf1, *buf2, *buf3;

    ctx=BN_CTX_new();
    if (ctx == NULL) goto err;
    
    ctx2=BN_CTX_new();
    if (ctx2 == NULL) goto err;
    
    r0 = &(ctx->bn[0]);
    r1 = &(ctx->bn[1]);
    r2 = &(ctx->bn[2]);
    r3 = &(ctx->bn[3]);
    ctx->tos += 4;

    bitsp = (bits+1)/2;
    bitsq = bits-bitsp;
    
    /* set e */
    e = BN_new();
    if (e == NULL) goto err;

    /* The problem is when building with 8, 16, or 32 BN_ULONG,
     * unsigned long can be larger */
    for (i=0; i<sizeof(unsigned long)*8; i++)
    {
        if (e_value & (1<<i))
            BN_set_bit(e,i);
    }

    /* generate p and q */
    for (;;)
    {
        p = BN_generate_prime (NULL, bitsp, 1, NULL, NULL, NULL, NULL);
        if (p == NULL) goto err;
        if (!BN_sub (r2, p, BN_value_one())) goto err;
        if (!BN_gcd (r1, r2, e, ctx)) goto err;
        if (BN_is_one(r1)) break;
        BN_free(p);
    }
    
    for (;;)
    {
        q = BN_generate_prime (NULL, bitsq, 1, NULL, NULL, NULL, NULL);
        if (q == NULL) goto err;
        if (!BN_sub (r2, q, BN_value_one())) goto err;
        if (!BN_gcd (r1, r2, e, ctx)) goto err;
        if (BN_is_one (r1) && (BN_cmp (p, q) != 0)) break;
        BN_free(q);
    }
    
    if (BN_cmp (p, q) < 0) tmp = p, p = q, q = tmp;

    /* calculate n */
    n = BN_new();
    if (n == NULL) goto err;
    if (!BN_mul (n, p, q, ctx)) goto err;

    /* calculate d */
    if (!BN_sub (r1, p, BN_value_one())) goto err;	/* p-1 */
    if (!BN_sub (r2, q, BN_value_one())) goto err;	/* q-1 */
    if (!BN_mul (r0, r1, r2, ctx)) goto err;	/* (p-1)(q-1) */

    d = BN_mod_inverse (NULL, e, r0, ctx2);	/* d */
    if (d == NULL) goto err;

    BN_CTX_free(ctx);
    BN_CTX_free(ctx2);
    
    // n, d, e are ready. secret key: n:d, public key: n:e
    buf1 = BN_bn2hex (n);
    buf2 = BN_bn2hex (d);
    buf3 = BN_bn2hex (e);

    *secret_key = malloc (strlen(buf1) + strlen(buf2) + 2);
    *public_key = malloc (strlen(buf1) + strlen(buf3) + 2);
    
    strcpy (*secret_key, buf1); strcat (*secret_key, ":"); strcat (*secret_key, buf2);
    strcpy (*public_key, buf1); strcat (*public_key, ":"); strcat (*public_key, buf3);
    free (buf1); free (buf2); free (buf3);

    // cleanup
    BN_clear_free (n);    BN_clear_free (e);
    BN_clear_free (d);    BN_clear_free (p);
    BN_clear_free (q);

    return 0;
    
err:
    
    BN_CTX_free(ctx);
    BN_CTX_free(ctx2);
    return -1;
}

