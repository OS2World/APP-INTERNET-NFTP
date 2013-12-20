EXTRA_DIST		= bootstrap.sh build-os2.cmd build-unix.sh \
  build-win32.bat Makefile.os2 m4 LICENSE
SUBDIRS 		= src tests
MAINTAINERCLEANFILES 	= Makefile.in configure

ACLOCAL_AMFLAGS = -I m4

test: all
	cd tests ;\
	$(MAKE) test
