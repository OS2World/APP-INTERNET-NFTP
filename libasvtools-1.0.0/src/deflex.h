#ifndef __DEFLEX_H__
#define __DEFLEX_H__

/* remember to edit when adding! this is total number of lexeme types
 plus one (type 0 is unused). don't forget to edit deflex.c as well */
#define NUM_LEXTYPES    25

#define LATWORD		1
#define CYRWORD		2
#define UWORD		3
#define EMAIL		4
#define FURL		5
#define HOST		6
#define SCIENTIFIC	7
#define VERSIONNUMBER	8
#define PARTHYPHENWORD 	9	
#define CYRPARTHYPHENWORD 	10	
#define LATPARTHYPHENWORD 	11	
#define SPACE 		12
#define TAG 		13
#define HTTP 		14
#define HYPHENWORD	15
#define LATHYPHENWORD	16
#define CYRHYPHENWORD	17
#define URI		18
#define FILEPATH	19
#define DECIMAL		20
#define SIGNEDINT	21
#define UNSIGNEDINT	22
#define HTMLENTITY	23
#define COMPLEXWORD     24

extern char *lextype_descriptions[];
extern char *lextype_names[];

#endif
