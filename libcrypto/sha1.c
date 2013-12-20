/* crypto/sha/sha1dgst.c */
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
#include <string.h>
#include <stdlib.h>
#include <asvtools.h>

#include "sha1.h"

#undef c2nl
#define c2nl(c,l)	(l =(((unsigned long)(*((c)++)))<<24), \
			 l|=(((unsigned long)(*((c)++)))<<16), \
			 l|=(((unsigned long)(*((c)++)))<< 8), \
			 l|=(((unsigned long)(*((c)++)))    ))

#undef p_c2nl
#define p_c2nl(c,l,n)	{ \
			switch (n) { \
			case 0: l =((unsigned long)(*((c)++)))<<24; \
			case 1: l|=((unsigned long)(*((c)++)))<<16; \
			case 2: l|=((unsigned long)(*((c)++)))<< 8; \
			case 3: l|=((unsigned long)(*((c)++))); \
				} \
			}

#undef c2nl_p
/* NOTE the pointer is not incremented at the end of this */
#define c2nl_p(c,l,n)	{ \
			l=0; \
			(c)+=n; \
			switch (n) { \
			case 3: l =((unsigned long)(*(--(c))))<< 8; \
			case 2: l|=((unsigned long)(*(--(c))))<<16; \
			case 1: l|=((unsigned long)(*(--(c))))<<24; \
				} \
			}

#undef p_c2nl_p
#define p_c2nl_p(c,l,sc,len) { \
			switch (sc) \
				{ \
			case 0: l =((unsigned long)(*((c)++)))<<24; \
				if (--len == 0) break; \
			case 1: l|=((unsigned long)(*((c)++)))<<16; \
				if (--len == 0) break; \
			case 2: l|=((unsigned long)(*((c)++)))<< 8; \
				} \
			}

#undef nl2c
#define nl2c(l,c)	(*((c)++)=(unsigned char)(((l)>>24)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16)&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
			 *((c)++)=(unsigned char)(((l)    )&0xff))

#undef c2l
#define c2l(c,l)	(l =(((unsigned long)(*((c)++)))    ), \
			 l|=(((unsigned long)(*((c)++)))<< 8), \
			 l|=(((unsigned long)(*((c)++)))<<16), \
			 l|=(((unsigned long)(*((c)++)))<<24))

#undef p_c2l
#define p_c2l(c,l,n)	{ \
			switch (n) { \
			case 0: l =((unsigned long)(*((c)++))); \
			case 1: l|=((unsigned long)(*((c)++)))<< 8; \
			case 2: l|=((unsigned long)(*((c)++)))<<16; \
			case 3: l|=((unsigned long)(*((c)++)))<<24; \
				} \
			}

#undef c2l_p
/* NOTE the pointer is not incremented at the end of this */
#define c2l_p(c,l,n)	{ \
			l=0; \
			(c)+=n; \
			switch (n) { \
			case 3: l =((unsigned long)(*(--(c))))<<16; \
			case 2: l|=((unsigned long)(*(--(c))))<< 8; \
			case 1: l|=((unsigned long)(*(--(c)))); \
				} \
			}

#undef p_c2l_p
#define p_c2l_p(c,l,sc,len) { \
			switch (sc) \
				{ \
			case 0: l =((unsigned long)(*((c)++))); \
				if (--len == 0) break; \
			case 1: l|=((unsigned long)(*((c)++)))<< 8; \
				if (--len == 0) break; \
			case 2: l|=((unsigned long)(*((c)++)))<<16; \
				} \
			}

#undef l2c
#define l2c(l,c)	(*((c)++)=(unsigned char)(((l)    )&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>24)&0xff))

#ifndef SHA_LONG_LOG2
#define SHA_LONG_LOG2	2	/* default to 32 bits */
#endif

#undef ROTATE
#undef Endian_Reverse32
#if defined(WIN32)
#define ROTATE(a,n)     _lrotl(a,n)
#elif defined(__GNUC__) && !defined(PEDANTIC)
/* some inline assembler templates by <appro@fy.chalmers.se> */
#if defined(__i386) && !defined(NO_ASM)
#define ROTATE(a,n)	({ register unsigned int ret;	\
				asm ("roll %1,%0"	\
				: "=r"(ret)		\
				: "I"(n), "0"(a)	\
				: "cc");		\
			   ret;				\
			})
#ifndef I386_ONLY
#define Endian_Reverse32(a) \
			{ register unsigned int ltmp=(a);	\
				asm ("bswapl %0"	\
				: "=r"(ltmp) : "0"(ltmp));	\
			  (a)=ltmp;			\
			}
#endif
#elif defined(__powerpc)
#define ROTATE(a,n)	({ register unsigned int ret;		\
				asm ("rlwinm %0,%1,%2,0,31"	\
				: "=r"(ret)			\
				: "r"(a), "I"(n));		\
			   ret;					\
			})
/* Endian_Reverse32 is not needed for PowerPC */
#endif
#endif

/* A nice byte order reversal from Wei Dai <weidai@eskimo.com> */
#ifdef ROTATE
#ifndef Endian_Reverse32
/* 5 instructions with rotate instruction, else 9 */
#define Endian_Reverse32(a) \
	{ \
	unsigned long t=(a); \
	(a)=((ROTATE(t,8)&0x00FF00FF)|(ROTATE((t&0x00FF00FF),24))); \
	}
#endif
#else
#define ROTATE(a,n)     (((a)<<(n))|(((a)&0xffffffff)>>(32-(n))))
#ifndef Endian_Reverse32
/* 6 instructions with rotate instruction, else 8 */
#define Endian_Reverse32(a) \
	{ \
	unsigned long t=(a); \
	t=(((t>>8)&0x00FF00FF)|((t&0x00FF00FF)<<8)); \
	(a)=ROTATE(t,16); \
	}
#endif
/*
 * Originally the middle line started with l=(((l&0xFF00FF00)>>8)|...
 * It's rewritten as above for two reasons:
 *	- RISCs aren't good at long constants and have to explicitely
 *	  compose 'em with several (well, usually 2) instructions in a
 *	  register before performing the actual operation and (as you
 *	  already realized:-) having same constant should inspire the
 *	  compiler to permanently allocate the only register for it;
 *	- most modern CPUs have two ALUs, but usually only one has
 *	  circuitry for shifts:-( this minor tweak inspires compiler
 *	  to schedule shift instructions in a better way...
 *
 *				<appro@fy.chalmers.se>
 */
#endif

/* As  pointed out by Wei Dai <weidai@eskimo.com>, F() below can be
 * simplified to the code in F_00_19.  Wei attributes these optimisations
 * to Peter Gutmann's SHS code, and he attributes it to Rich Schroeppel.
 * #define F(x,y,z) (((x) & (y))  |  ((~(x)) & (z)))
 * I've just become aware of another tweak to be made, again from Wei Dai,
 * in F_40_59, (x&a)|(y&a) -> (x|y)&a
 */
#define	F_00_19(b,c,d)	((((c) ^ (d)) & (b)) ^ (d)) 
#define	F_20_39(b,c,d)	((b) ^ (c) ^ (d))
#define F_40_59(b,c,d)	(((b) & (c)) | (((b)|(c)) & (d))) 
#define	F_60_79(b,c,d)	F_20_39(b,c,d)

#undef Xupdate
#define Xupdate(a,i,ia,ib,ic,id) (a)=\
	(ia[(i)&0x0f]^ib[((i)+2)&0x0f]^ic[((i)+8)&0x0f]^id[((i)+13)&0x0f]);\
	X[(i)&0x0f]=(a)=ROTATE((a),1);

#define BODY_00_15(i,a,b,c,d,e,f,xa) \
	(f)=xa[i]+(e)+K_00_19+ROTATE((a),5)+F_00_19((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_16_19(i,a,b,c,d,e,f,xa,xb,xc,xd) \
	Xupdate(f,i,xa,xb,xc,xd); \
	(f)+=(e)+K_00_19+ROTATE((a),5)+F_00_19((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_20_31(i,a,b,c,d,e,f,xa,xb,xc,xd) \
	Xupdate(f,i,xa,xb,xc,xd); \
	(f)+=(e)+K_20_39+ROTATE((a),5)+F_20_39((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_32_39(i,a,b,c,d,e,f,xa) \
	Xupdate(f,i,xa,xa,xa,xa); \
	(f)+=(e)+K_20_39+ROTATE((a),5)+F_20_39((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_40_59(i,a,b,c,d,e,f,xa) \
	Xupdate(f,i,xa,xa,xa,xa); \
	(f)+=(e)+K_40_59+ROTATE((a),5)+F_40_59((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_60_79(i,a,b,c,d,e,f,xa) \
	Xupdate(f,i,xa,xa,xa,xa); \
	(f)=X[(i)&0x0f]+(e)+K_60_79+ROTATE((a),5)+F_60_79((b),(c),(d)); \
	(b)=ROTATE((b),30);

/* Implemented from SHA-1 document - The Secure Hash Algorithm
 */

#define INIT_DATA_h0 0x67452301UL
#define INIT_DATA_h1 0xefcdab89UL
#define INIT_DATA_h2 0x98badcfeUL
#define INIT_DATA_h3 0x10325476UL
#define INIT_DATA_h4 0xc3d2e1f0UL

#define K_00_19	0x5a827999UL
#define K_20_39 0x6ed9eba1UL
#define K_40_59 0x8f1bbcdcUL
#define K_60_79 0xca62c1d6UL

static void sha1_block(SHA_CTX *c, register SHA_LONG *p, int num);

#if !defined(B_ENDIAN) && defined(SHA1_ASM)
#  define	M_c2nl 		c2l
#  define	M_p_c2nl 	p_c2l
#  define	M_c2nl_p	c2l_p
#  define	M_p_c2nl_p	p_c2l_p
#  define	M_nl2c		l2c
#else
#  define	M_c2nl 		c2nl
#  define	M_p_c2nl	p_c2nl
#  define	M_c2nl_p	c2nl_p
#  define	M_p_c2nl_p	p_c2nl_p
#  define	M_nl2c		nl2c
#endif

void SHA1_Init(SHA_CTX *c)
	{
	c->h0=INIT_DATA_h0;
	c->h1=INIT_DATA_h1;
	c->h2=INIT_DATA_h2;
	c->h3=INIT_DATA_h3;
	c->h4=INIT_DATA_h4;
	c->Nl=0;
	c->Nh=0;
	c->num=0;
	}

void SHA1_Update(SHA_CTX *c, register const unsigned char *data,
	     unsigned long len)
	{
	register SHA_LONG *p;
	int ew,ec,sw,sc;
	SHA_LONG l;

	if (len == 0) return;

	l=(c->Nl+(len<<3))&0xffffffffL;
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh+=(len>>29);
	c->Nl=l;

	if (c->num != 0)
		{
		p=c->data;
		sw=c->num>>2;
		sc=c->num&0x03;

		if ((c->num+len) >= SHA_CBLOCK)
			{
			l= p[sw];
			M_p_c2nl(data,l,sc);
			p[sw++]=l;
			for (; sw<SHA_LBLOCK; sw++)
				{
				M_c2nl(data,l);
				p[sw]=l;
				}
			len-=(SHA_CBLOCK-c->num);

			sha1_block(c,p,1);
			c->num=0;
			/* drop through and do the rest */
			}
		else
			{
			c->num+=(int)len;
			if ((sc+len) < 4) /* ugly, add char's to a word */
				{
				l= p[sw];
				M_p_c2nl_p(data,l,sc,len);
				p[sw]=l;
				}
			else
				{
				ew=(c->num>>2);
				ec=(c->num&0x03);
				l= p[sw];
				M_p_c2nl(data,l,sc);
				p[sw++]=l;
				for (; sw < ew; sw++)
					{ M_c2nl(data,l); p[sw]=l; }
				if (ec)
					{
					M_c2nl_p(data,l,ec);
					p[sw]=l;
					}
				}
			return;
			}
		}
	/* We can only do the following code for assember, the reason
	 * being that the sha1_block 'C' version changes the values
	 * in the 'data' array.  The assember code avoids this and
	 * copies it to a local array.  I should be able to do this for
	 * the C version as well....
	 */
#if SHA_LONG_LOG2==2
#if defined(B_ENDIAN) || defined(SHA1_ASM)
	if ((((unsigned long)data)%sizeof(SHA_LONG)) == 0)
		{
		sw=len/SHA_CBLOCK;
		if (sw)
			{
			sha1_block(c,(SHA_LONG *)data,sw);
			sw*=SHA_CBLOCK;
			data+=sw;
			len-=sw;
			}
		}
#endif
#endif
	/* we now can process the input data in blocks of SHA_CBLOCK
	 * chars and save the leftovers to c->data. */
	p=c->data;
	while (len >= SHA_CBLOCK)
		{
#if SHA_LONG_LOG2==2
#if defined(B_ENDIAN) || defined(SHA1_ASM)
#define SHA_NO_TAIL_CODE
		/*
		 * Basically we get here only when data happens
		 * to be unaligned.
		 */
		if (p != (SHA_LONG *)data)
			memcpy(p,data,SHA_CBLOCK);
		data+=SHA_CBLOCK;
		sha1_block(c,p=c->data,1);
		len-=SHA_CBLOCK;
#elif defined(L_ENDIAN)
#define BE_COPY(dst,src,i)	{				\
				l = ((SHA_LONG *)src)[i];	\
				Endian_Reverse32(l);		\
				dst[i] = l;			\
				}
		if ((((unsigned long)data)%sizeof(SHA_LONG)) == 0)
			{
			for (sw=(SHA_LBLOCK/4); sw; sw--)
				{
				BE_COPY(p,data,0);
				BE_COPY(p,data,1);
				BE_COPY(p,data,2);
				BE_COPY(p,data,3);
				p+=4;
				data += 4*sizeof(SHA_LONG);
				}
			sha1_block(c,p=c->data,1);
			len-=SHA_CBLOCK;
			continue;
			}
#endif
#endif
#ifndef SHA_NO_TAIL_CODE
		/*
		 * In addition to "sizeof(SHA_LONG)!= 4" case the
		 * following code covers unaligned access cases on
		 * little-endian machines.
		 *			<appro@fy.chalmers.se>
		 */
		p=c->data;
		for (sw=(SHA_LBLOCK/4); sw; sw--)
			{
			M_c2nl(data,l); p[0]=l;
			M_c2nl(data,l); p[1]=l;
			M_c2nl(data,l); p[2]=l;
			M_c2nl(data,l); p[3]=l;
			p+=4;
			}
		p=c->data;
		sha1_block(c,p,1);
		len-=SHA_CBLOCK;
#endif
		}
	ec=(int)len;
	c->num=ec;
	ew=(ec>>2);
	ec&=0x03;

	for (sw=0; sw < ew; sw++)
		{ M_c2nl(data,l); p[sw]=l; }
	M_c2nl_p(data,l,ec);
	p[sw]=l;
	}

void SHA1_Transform(SHA_CTX *c, unsigned char *b)
	{
	SHA_LONG p[SHA_LBLOCK];

#if SHA_LONG_LOG2==2
#if defined(B_ENDIAN) || defined(SHA1_ASM)
	memcpy(p,b,SHA_CBLOCK);
	sha1_block(c,p,1);
	return;
#elif defined(L_ENDIAN)
	if (((unsigned long)b%sizeof(SHA_LONG)) == 0)
		{
		SHA_LONG *q;
		int i;

		q=p;
		for (i=(SHA_LBLOCK/4); i; i--)
			{
			unsigned long l;
			BE_COPY(q,b,0);	/* BE_COPY was defined above */
			BE_COPY(q,b,1);
			BE_COPY(q,b,2);
			BE_COPY(q,b,3);
			q+=4;
			b+=4*sizeof(SHA_LONG);
			}
		sha1_block(c,p,1);
		return;
		}
#endif
#endif
#ifndef SHA_NO_TAIL_CODE /* defined above, see comment */
		{
		SHA_LONG *q;
		int i;
	
		q=p;
		for (i=(SHA_LBLOCK/4); i; i--)
			{
			SHA_LONG l;
			c2nl(b,l); *(q++)=l;
			c2nl(b,l); *(q++)=l;
			c2nl(b,l); *(q++)=l;
			c2nl(b,l); *(q++)=l; 
			} 
		sha1_block(c,p,1);
		}
#endif
	}

static void sha1_block(SHA_CTX *c, register SHA_LONG *W, int num)
	{
	register SHA_LONG A,B,C,D,E,T;
	SHA_LONG X[SHA_LBLOCK];

	A=c->h0;
	B=c->h1;
	C=c->h2;
	D=c->h3;
	E=c->h4;

	for (;;)
		{
	BODY_00_15( 0,A,B,C,D,E,T,W);
	BODY_00_15( 1,T,A,B,C,D,E,W);
	BODY_00_15( 2,E,T,A,B,C,D,W);
	BODY_00_15( 3,D,E,T,A,B,C,W);
	BODY_00_15( 4,C,D,E,T,A,B,W);
	BODY_00_15( 5,B,C,D,E,T,A,W);
	BODY_00_15( 6,A,B,C,D,E,T,W);
	BODY_00_15( 7,T,A,B,C,D,E,W);
	BODY_00_15( 8,E,T,A,B,C,D,W);
	BODY_00_15( 9,D,E,T,A,B,C,W);
	BODY_00_15(10,C,D,E,T,A,B,W);
	BODY_00_15(11,B,C,D,E,T,A,W);
	BODY_00_15(12,A,B,C,D,E,T,W);
	BODY_00_15(13,T,A,B,C,D,E,W);
	BODY_00_15(14,E,T,A,B,C,D,W);
	BODY_00_15(15,D,E,T,A,B,C,W);
	BODY_16_19(16,C,D,E,T,A,B,W,W,W,W);
	BODY_16_19(17,B,C,D,E,T,A,W,W,W,W);
	BODY_16_19(18,A,B,C,D,E,T,W,W,W,W);
	BODY_16_19(19,T,A,B,C,D,E,W,W,W,X);

	BODY_20_31(20,E,T,A,B,C,D,W,W,W,X);
	BODY_20_31(21,D,E,T,A,B,C,W,W,W,X);
	BODY_20_31(22,C,D,E,T,A,B,W,W,W,X);
	BODY_20_31(23,B,C,D,E,T,A,W,W,W,X);
	BODY_20_31(24,A,B,C,D,E,T,W,W,X,X);
	BODY_20_31(25,T,A,B,C,D,E,W,W,X,X);
	BODY_20_31(26,E,T,A,B,C,D,W,W,X,X);
	BODY_20_31(27,D,E,T,A,B,C,W,W,X,X);
	BODY_20_31(28,C,D,E,T,A,B,W,W,X,X);
	BODY_20_31(29,B,C,D,E,T,A,W,W,X,X);
	BODY_20_31(30,A,B,C,D,E,T,W,X,X,X);
	BODY_20_31(31,T,A,B,C,D,E,W,X,X,X);
	BODY_32_39(32,E,T,A,B,C,D,X);
	BODY_32_39(33,D,E,T,A,B,C,X);
	BODY_32_39(34,C,D,E,T,A,B,X);
	BODY_32_39(35,B,C,D,E,T,A,X);
	BODY_32_39(36,A,B,C,D,E,T,X);
	BODY_32_39(37,T,A,B,C,D,E,X);
	BODY_32_39(38,E,T,A,B,C,D,X);
	BODY_32_39(39,D,E,T,A,B,C,X);

	BODY_40_59(40,C,D,E,T,A,B,X);
	BODY_40_59(41,B,C,D,E,T,A,X);
	BODY_40_59(42,A,B,C,D,E,T,X);
	BODY_40_59(43,T,A,B,C,D,E,X);
	BODY_40_59(44,E,T,A,B,C,D,X);
	BODY_40_59(45,D,E,T,A,B,C,X);
	BODY_40_59(46,C,D,E,T,A,B,X);
	BODY_40_59(47,B,C,D,E,T,A,X);
	BODY_40_59(48,A,B,C,D,E,T,X);
	BODY_40_59(49,T,A,B,C,D,E,X);
	BODY_40_59(50,E,T,A,B,C,D,X);
	BODY_40_59(51,D,E,T,A,B,C,X);
	BODY_40_59(52,C,D,E,T,A,B,X);
	BODY_40_59(53,B,C,D,E,T,A,X);
	BODY_40_59(54,A,B,C,D,E,T,X);
	BODY_40_59(55,T,A,B,C,D,E,X);
	BODY_40_59(56,E,T,A,B,C,D,X);
	BODY_40_59(57,D,E,T,A,B,C,X);
	BODY_40_59(58,C,D,E,T,A,B,X);
	BODY_40_59(59,B,C,D,E,T,A,X);

	BODY_60_79(60,A,B,C,D,E,T,X);
	BODY_60_79(61,T,A,B,C,D,E,X);
	BODY_60_79(62,E,T,A,B,C,D,X);
	BODY_60_79(63,D,E,T,A,B,C,X);
	BODY_60_79(64,C,D,E,T,A,B,X);
	BODY_60_79(65,B,C,D,E,T,A,X);
	BODY_60_79(66,A,B,C,D,E,T,X);
	BODY_60_79(67,T,A,B,C,D,E,X);
	BODY_60_79(68,E,T,A,B,C,D,X);
	BODY_60_79(69,D,E,T,A,B,C,X);
	BODY_60_79(70,C,D,E,T,A,B,X);
	BODY_60_79(71,B,C,D,E,T,A,X);
	BODY_60_79(72,A,B,C,D,E,T,X);
	BODY_60_79(73,T,A,B,C,D,E,X);
	BODY_60_79(74,E,T,A,B,C,D,X);
	BODY_60_79(75,D,E,T,A,B,C,X);
	BODY_60_79(76,C,D,E,T,A,B,X);
	BODY_60_79(77,B,C,D,E,T,A,X);
	BODY_60_79(78,A,B,C,D,E,T,X);
	BODY_60_79(79,T,A,B,C,D,E,X);
	
	c->h0=(c->h0+E)&0xffffffffL; 
	c->h1=(c->h1+T)&0xffffffffL;
	c->h2=(c->h2+A)&0xffffffffL;
	c->h3=(c->h3+B)&0xffffffffL;
	c->h4=(c->h4+C)&0xffffffffL;

	if (--num <= 0) break;

	A=c->h0;
	B=c->h1;
	C=c->h2;
	D=c->h3;
	E=c->h4;

	W+=SHA_LBLOCK;	/* Note! This can happen only when sizeof(SHA_LONG)
			 * is 4. Whenever it's not the actual case this
			 * function is never called with num larger than 1
			 * and we never advance down here.
			 *			<appro@fy.chalmers.se>
			 */
		}
	}

void SHA1_Final(unsigned char *md, SHA_CTX *c)
	{
	register int i,j;
	register SHA_LONG l;
	register SHA_LONG *p;
	static unsigned char end[4]={0x80,0x00,0x00,0x00};
	unsigned char *cp=end;

	/* c->num should definitly have room for at least one more byte. */
	p=c->data;
	j=c->num;
	i=j>>2;
	l=p[i];
	M_p_c2nl(cp,l,j&0x03);
	p[i]=l;
	i++;
	/* i is the next 'undefined word' */
	if (c->num >= SHA_LAST_BLOCK)
		{
		for (; i<SHA_LBLOCK; i++)
			p[i]=0;
		sha1_block(c,p,1);
		i=0;
		}
	for (; i<(SHA_LBLOCK-2); i++)
		p[i]=0;
	p[SHA_LBLOCK-2]=c->Nh;
	p[SHA_LBLOCK-1]=c->Nl;
#if SHA_LONG_LOG2==2
#if !defined(B_ENDIAN) && defined(SHA1_ASM)
	Endian_Reverse32(p[SHA_LBLOCK-2]);
	Endian_Reverse32(p[SHA_LBLOCK-1]);
#endif
#endif
	sha1_block(c,p,1);
	cp=md;
	l=c->h0; nl2c(l,cp);
	l=c->h1; nl2c(l,cp);
	l=c->h2; nl2c(l,cp);
	l=c->h3; nl2c(l,cp);
	l=c->h4; nl2c(l,cp);

	c->num=0;
	/* sha_block may be leaving some stuff on the stack
	 * but I'm not worried :-)
	memset((void *)c,0,sizeof(SHA_CTX));
	 */
	}

unsigned char *sha1(const unsigned char *d, unsigned long n, unsigned char *md)
	{
	SHA_CTX c;
	static unsigned char m[SHA_DIGEST_LENGTH];

	if (md == NULL) md=m;
	SHA1_Init(&c);
	SHA1_Update(&c,d,n);
	SHA1_Final(md,&c);
	memset(&c,0,sizeof(c));
	return(md);
	}

/* computes SHA1 digest. returns pointer to malloc()ed buffer of 41 bytes
 (the digest) in hexadecimal form */
char *sha1x (char *s)
{
    unsigned char hash[20];
    
    sha1 ((unsigned char *)s, strlen(s), hash);
    
    return bin2hex (hash, 20);
}
