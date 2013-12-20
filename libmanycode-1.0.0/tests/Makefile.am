EXTRA_DIST = Makefile.os2

noinst_PROGRAMS = ruscode$(EXEEXT) trigrams$(EXEEXT) alphabet$(EXEEXT) 

LDADD = -L$(top_srcdir)/src -lmanycode
AM_CPPFLAGS = -I$(top_srcdir)/src

LDADD += -lasvtools

test: all
	@ for p in $(noinst_PROGRAMS); do \
		echo "....$$p" ;\
		$$p$(EXEEXT) ;\
	done
