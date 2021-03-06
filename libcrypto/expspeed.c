/* crypto/bn/expspeed.c */
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

/* most of this code has been pilfered from my libdes speed.c program */

#define BASENUM	50

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <asvtools.h>

#include "bn.h"

int run=0;

#define NUM_SIZES       7
static int sizes[NUM_SIZES]={256,512,768,1024,2048,4096,8192};

void do_mul_exp(BIGNUM *r,BIGNUM *a,BIGNUM *b,BIGNUM *c,BN_CTX *ctx); 

int main(int argc, char **argv)
	{
	BN_CTX *ctx;
	BIGNUM *a,*b,*c,*r;

	ctx=BN_CTX_new();
	a=BN_new();
	b=BN_new();
	c=BN_new();
	r=BN_new();

        do_mul_exp(r,a,b,c,ctx);
        return 0;
	}

void do_mul_exp(BIGNUM *r, BIGNUM *a, BIGNUM *b, BIGNUM *c, BN_CTX *ctx)
	{
	int i,k;
	double t1, t2;
	long num;
	BN_MONT_CTX m;

	memset(&m,0,sizeof(m));

	num=BASENUM;
	for (i=0; i<NUM_SIZES; i++)
		{
		BN_rand(a,sizes[i],1,0);
		BN_rand(b,sizes[i],1,0);
		BN_rand(c,sizes[i],1,1);
		BN_mod(a,a,c,ctx);
		BN_mod(b,b,c,ctx);

		BN_MONT_CTX_set(&m,c,ctx);

                t1 = clock1 ();
		for (k=0; k<num; k++)
			BN_mod_exp_mont(r,a,b,c,ctx,&m);
                t2 = clock1 ();
		printf("mul %4d ^ %4d %% %4d -> %10.5f sec\n",sizes[i],sizes[i],sizes[i],(t2-t1)/num);
		num/=7;
		if (num <= 0) num=1;
		}

	}

