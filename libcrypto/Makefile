DIST_NAME=libcrypto
DIST_VERSION=1.0

PROJECT = scl
OBJ = rsa1.o rsa2.o elgamal.o bn1.o bn2.o rand.o sha1.o blowfish.o
#OBJ = rsa1.o rsa2.o elgamal.o rand.o sha1.o blowfish.o
LIBS1 = -lasvtools
CFLAGS1 = -g -O

include Makefile.inc

test: expspeed.exe rsatest.exe egtest.exe

expspeed.exe: expspeed.o libscl.a
	$(CC) $(LDFLAGS) -Zcrtdll -o $@ $^ -lasvtools

rsatest.exe: rsatest.o libscl.a
	$(CC) $(LDFLAGS) -Zcrtdll -o $@ $^ -lasvtools

egtest.exe: egtest.o libscl.a
	$(CC) $(LDFLAGS) -Zcrtdll -o $@ $^ -lasvtools

# -- producing source package for PKFA ------

SOURCE = bn.h bn_lcl.h sha1.h \
  ../asvtools-1.0/src/hex2bin.c ../asvtools-1.0/src/str_numchars.c \
  bn1.c bn2.c elgamal.c rand.c rsa1.c rsa2.c sha1.c
 
FILTER = fgrep -v "bn_lcl.h" | fgrep -v "bn.h" | fgrep -v "sha1.h" | fgrep -v "asvtools.h"

scl:
	cat $(SOURCE) | $(FILTER) >scl.c

