DIST_NAME=libftpsearch
DIST_VERSION=1.0

ifdef RUNWORKPLACE
    LIBS2 = -lsocket
endif

PROJECT = fsearch
OBJ = fsearch.o ardp.o
LIBS1 = -lasvtools $(LIBS2)
CFLAGS1 =
CLEANFILES = results.tmp debug.txt 

include Makefile.inc

.PHONY: test

test: ftps.exe

ftps.exe: ftps.o lib$(PROJECT).a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS1)

