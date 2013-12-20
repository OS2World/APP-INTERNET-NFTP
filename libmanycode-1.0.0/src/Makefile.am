EXTRA_DIST = ic Makefile.os2 ucm2ic.c

lib_LTLIBRARIES 	= libmanycode.la

include_HEADERS = manycode.h

libmanycode_la_SOURCES 	= manycode.h ucs_tables.h charsets.h unicode.tab \
	charsets.c ucs.c ucm_parse.c utf8.c ucs_tables.c koi8_ctype.c \
	ru_tables.c ruscode.c ucs_tables.h trigrams.kov

libmanycode_la_LIBADD = -lasvtools

BUILT_SOURCES = unicode.tab
noinst_PROGRAMS = mktab$(EXEEXT)
mktab_SOURCES = mktab.c

MAINTAINERCLEANFILES	= Makefile.in
CLEANFILES		= unicode.tab 

CODEPAGES = 437 813 850 852 855 857 860 861 862 863 864 \
 865 866 869 878 912 913 914 915 916 920 1250 1251 1252 1253 1254 \
 1255 1256 1257 1275 1280 1282 1283

unicode.tab: mktab$(EXEEXT)
	./mktab$(EXEEXT) $(CODEPAGES) > unicode.tab
