/* crypto/rand/md_rand.c */
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

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "sha1.h"
#include "scl.h"

#ifdef __MINGW32__
#include <process.h>
#endif

#define DEVRANDOM "/dev/urandom"

/* Changed how the state buffer used.  I now attempt to 'wrap' such
 * that I don't run over the same locations the next time  go through
 * the 1023 bytes - many thanks to
 * Robert J. LeBlanc <rjl@renaissoft.com> for his comments
 */

#define STATE_SIZE	1023
static int state_num=0,state_index=0;
static unsigned char state[STATE_SIZE+SHA_DIGEST_LENGTH];
static unsigned char md[SHA_DIGEST_LENGTH];
static long md_count[2]={0,0};

void rand_cleanup(void)
{
    memset(state,0,sizeof(state));
    state_num=0;
    state_index=0;
    memset(md,0,SHA_DIGEST_LENGTH);
    md_count[0]=0;
    md_count[1]=0;
}

void rand_seed(const void *buf, int num)
{
    int i,j,k,st_idx,st_num;
    SHA_CTX m;

    st_idx=state_index;
    st_num=state_num;

    state_index=(state_index+num);
    if (state_index >= STATE_SIZE)
    {
        state_index%=STATE_SIZE;
        state_num=STATE_SIZE;
    }
    else if (state_num < STATE_SIZE)
    {
        if (state_index > state_num)
            state_num=state_index;
    }

    for (i=0; i<num; i+=SHA_DIGEST_LENGTH)
    {
        j=(num-i);
        j=(j > SHA_DIGEST_LENGTH)?SHA_DIGEST_LENGTH:j;

        SHA1_Init(&m);
        SHA1_Update(&m,md,SHA_DIGEST_LENGTH);
        k=(st_idx+j)-STATE_SIZE;
        if (k > 0)
        {
            SHA1_Update(&m,&(state[st_idx]),j-k);
            SHA1_Update(&m,&(state[0]),k);
        }
        else
            SHA1_Update(&m,&(state[st_idx]),j);

        SHA1_Update(&m,buf,j);
        SHA1_Update(&m,(unsigned char *)&(md_count[0]),sizeof(md_count));
        SHA1_Final(md,&m);
        md_count[1]++;

        buf=(const char *)buf + j;

        for (k=0; k<j; k++)
        {
            state[st_idx++]^=md[k];
            if (st_idx >= STATE_SIZE)
            {
                st_idx=0;
                st_num=STATE_SIZE;
            }
        }
    }
    memset((char *)&m,0,sizeof(m));
}

void rand_bytes(unsigned char *buf, int num)
{
    int i,j,k,st_num,st_idx;
    SHA_CTX m;
    static int init=1;
    unsigned long l;
    FILE *fh;

    if (init)
    {
        /* put in some default random data, we need more than
         * just this */
        rand_seed (&m, sizeof(m));
        l = getpid ();
        rand_seed (&l, sizeof(l));
#ifndef __WIN32__
        l = getppid ();
        rand_seed (&l, sizeof(l));
#endif
        l = time (NULL);
        rand_seed (&l,sizeof(l));

        /*
         * Use a random entropy pool device.
         * Linux 1.3.x and FreeBSD-Current has
         * this. Use /dev/urandom if you can
         * as /dev/random will block if it runs out
         * of random entries.
         */
        if ((fh = fopen(DEVRANDOM, "r")) != NULL)
        {
            unsigned char tmpbuf[32];

            fread((unsigned char *)tmpbuf,1,32,fh);
            /* we don't care how many bytes we read,
             * we will just copy the 'stack' if there is
             * nothing else :-) */
            fclose(fh);
            rand_seed(tmpbuf,32);
            memset(tmpbuf,0,32);
        }
        init=0;
    }

    st_idx=state_index;
    st_num=state_num;
    state_index+=num;
    if (state_index > state_num)
        state_index=(state_index%state_num);

    while (num > 0)
    {
        j=(num >= SHA_DIGEST_LENGTH/2)?SHA_DIGEST_LENGTH/2:num;
        num-=j;
        SHA1_Init(&m);
        SHA1_Update(&m,&(md[SHA_DIGEST_LENGTH/2]),SHA_DIGEST_LENGTH/2);
        SHA1_Update(&m,(unsigned char *)&(md_count[0]),sizeof(md_count));
        k=(st_idx+j)-st_num;
        if (k > 0)
        {
            SHA1_Update(&m,&(state[st_idx]),j-k);
            SHA1_Update(&m,&(state[0]),k);
        }
        else
            SHA1_Update(&m,&(state[st_idx]),j);
        SHA1_Final(md,&m);

        for (i=0; i<j; i++)
        {
            if (st_idx >= st_num)
                st_idx=0;
            state[st_idx++]^=md[i];
            *(buf++)=md[i+SHA_DIGEST_LENGTH/2];
        }
    }

    SHA1_Init(&m);
    SHA1_Update(&m,(unsigned char *)&(md_count[0]),sizeof(md_count));
    md_count[0]++;
    SHA1_Update(&m,md,SHA_DIGEST_LENGTH);
    SHA1_Final(md,&m);
    memset(&m,0,sizeof(m));
}

