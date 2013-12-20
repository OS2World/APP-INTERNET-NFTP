#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*-
 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)regerror.c	8.4 (Berkeley) 3/20/94
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)regerror.c	8.4 (Berkeley) 3/20/94";
#endif /* LIBC_SCCS and not lint */

#include "regex.h1"

/* === regerror.c === */
static char *regatoi (const regex1_t *preg, char *localbuf);

#ifdef __cplusplus
}
#endif
/* ========= end header generated by ./mkh ========= */
/*
 = #define	REG1_NOMATCH	 1
 = #define	REG1_BADPAT	 2
 = #define	REG1_ECOLLATE	 3
 = #define	REG1_ECTYPE	 4
 = #define	REG1_EESCAPE	 5
 = #define	REG1_ESUBREG	 6
 = #define	REG1_EBRACK	 7
 = #define	REG1_EPAREN	 8
 = #define	REG1_EBRACE	 9
 = #define	REG1_BADBR	10
 = #define	REG1_ERANGE	11
 = #define	REG1_ESPACE	12
 = #define	REG1_BADRPT	13
 = #define	REG1_EMPTY	14
 = #define	REG1_ASSERT	15
 = #define	REG1_INVARG	16
*/
/* = #define	REG1_ATOI	255 */	/* convert name to number (!) */
/* = #define	REG1_ITOA	0400i */	/* convert number to name (!) */
static struct rerr {
	int code;
	char *name;
	char *explain;
} rerrs[] = {
	{REG1_NOMATCH,	"REG1_NOMATCH",	"regexec() failed to match"},
	{REG1_BADPAT,	"REG1_BADPAT",	"invalid regular expression"},
	{REG1_ECOLLATE,	"REG1_ECOLLATE",	"invalid collating element"},
	{REG1_ECTYPE,	"REG1_ECTYPE",	"invalid character class"},
	{REG1_EESCAPE,	"REG1_EESCAPE",	"trailing backslash (\\)"},
	{REG1_ESUBREG,	"REG1_ESUBREG",	"invalid backreference number"},
	{REG1_EBRACK,	"REG1_EBRACK",	"brackets ([ ]) not balanced"},
	{REG1_EPAREN,	"REG1_EPAREN",	"parentheses not balanced"},
	{REG1_EBRACE,	"REG1_EBRACE",	"braces not balanced"},
	{REG1_BADBR,	"REG1_BADBR",	"invalid repetition count(s)"},
	{REG1_ERANGE,	"REG1_ERANGE",	"invalid character range"},
	{REG1_ESPACE,	"REG1_ESPACE",	"out of memory"},
	{REG1_BADRPT,	"REG1_BADRPT",	"repetition-operator operand invalid"},
	{REG1_EMPTY,	"REG1_EMPTY",	"empty (sub)expression"},
	{REG1_ASSERT,	"REG1_ASSERT",	"\"can't happen\" -- you found a bug"},
	{REG1_INVARG,	"REG1_INVARG",	"invalid argument to regex routine"},
	{0,		"",		"*** unknown regexp error code ***"}
};

/*
 - regerror - the interface to error numbers
 = extern size_t regerror(int, const regex1_t *, char *, size_t);
 */
/* ARGSUSED */
size_t
regerror1(errcode, preg, errbuf, errbuf_size)
int errcode;
const regex1_t *preg;
char *errbuf;
size_t errbuf_size;
{
	register struct rerr *r;
	register size_t len;
	register int target = errcode &~ REG1_ITOA;
	register char *s;
	char convbuf[50];

	if (errcode == REG1_ATOI)
		s = regatoi(preg, convbuf);
	else {
		for (r = rerrs; r->code != 0; r++)
			if (r->code == target)
				break;

		if (errcode&REG1_ITOA) {
			if (r->code != 0)
				(void) strcpy(convbuf, r->name);
			else
				sprintf(convbuf, "REG1_0x%x", target);
			assert(strlen(convbuf) < sizeof(convbuf));
			s = convbuf;
		} else
			s = r->explain;
	}

	len = strlen(s) + 1;
	if (errbuf_size > 0) {
		if (errbuf_size > len)
			(void) strcpy(errbuf, s);
		else {
			(void) strncpy(errbuf, s, errbuf_size-1);
			errbuf[errbuf_size-1] = '\0';
		}
	}

	return(len);
}

/*
 - regatoi - internal routine to implement REG1_ATOI
 == static char *regatoi(const regex1_t *preg, char *localbuf);
 */
static char *
regatoi(preg, localbuf)
const regex1_t *preg;
char *localbuf;
{
	register struct rerr *r;

	for (r = rerrs; r->code != 0; r++)
		if (strcmp(r->name, preg->re_endp) == 0)
			break;
	if (r->code == 0)
		return("0");

	sprintf(localbuf, "%d", r->code);
	return(localbuf);
}
