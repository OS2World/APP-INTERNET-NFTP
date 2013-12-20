
#---------------------------------------------------------------------------

ifeq ($(BE_HOST_CPU), x86)
INSTALL = install-beos
RANLIB = ar s
endif

ifdef windir
INSTALL = install-win32
LIBS2 = -lregex
RANLIB = ar s
endif

ifdef RUNWORKPLACE
INSTALL = install-os2
LIBS2 = -lbsd -lsocket -lregex
RANLIB = ar s
endif

ifndef INSTALL
INSTALL = install-unix
RANLIB = ranlib
endif

#---------------------------------------------------------------------------

CC = gcc
CFLAGS = -Wall -O -g -I../include $(CFLAGS1)
LDFLAGS = -L../lib
HEADER = $(PROJECT).h
LIBRARY = lib$(PROJECT).a

all: $(LIBRARY)

$(LIBRARY): $(OBJ)
	rm -f $@
	ar rc $@ $(OBJ)
	$(RANLIB) $@

install: $(INSTALL)

install-beos:
	mkdir -p /boot/develop/headers
	mkdir -p /boot/develop/lib/x86
	cp -f $(HEADER) /boot/develop/headers
	cp -f $(LIBRARY) /boot/develop/lib/x86

install-unix:
	mkdir -p ../include
	mkdir -p ../lib
	cp -f $(HEADER) ../include
	cp -f $(LIBRARY) ../lib

#install-os2:
#	@install-os2.cmd $(PROJECT)

install-os2:
	cp -f $(HEADER) ../include
	cp -f $(LIBRARY) ../lib/$(PROJECT).a

#install-win32:
#	@echo $(LIBRARY) installation
#	@echo .
#	@echo The following files will be installed:
#	@echo .
#	@echo C header file: C:/MINGW32/include/$(HEADER)
#	@echo .
#	@cp $(HEADER) C:/MINGW32/include/
#	@cp $(LIBRARY) C:/MINGW32/lib/
#	@echo Installation complete

install-win32:
	cp -f $(HEADER) ../include
	cp -f $(LIBRARY) ../lib

os2dll: $(OBJ)
	$(CC) -o $(PROJECT).dll -Zdll -Zcrtdll $(OBJ) $(LIBS1) $(PROJECT).def
	emxbind -qs $(PROJECT).dll
	emximp -o $(LIBRARY) $(PROJECT).def
	emxomf -s -l -q $(LIBRARY)

clean:
	rm -f *.o *.a *.lib *.dll *.so* core *.core *.exe
	rm -f $(PROJECT)_wrap.c $(PROJECT)_wrap.doc $(PROJECT).pm
	rm -f $(CLEANFILES)

ifeq ($(findstring freebsd,$(OSTYPE)), freebsd)
PERLINC = -I/usr/local/perl/lib/5.00503/i386-freebsd/CORE \
          -I/usr/libdata/perl/5.00503/mach/CORE \
          -I/usr/local/lib/perl5/5.6.0/i386-freebsd/CORE \
          -I/usr/local/perl/lib/5.6.0/i386-freebsd/CORE \
          -I/usr/libdata/perl/5.6.0/mach/CORE \
          -I/usr/local/lib/perl5/5.6.0/i386-freebsd/CORE
endif

ifeq ($(OSTYPE), FreeBSD)
PERLINC = -I/usr/local/perl/lib/5.00503/i386-freebsd/CORE \
          -I/usr/libdata/perl/5.00503/mach/CORE \
          -I/usr/local/lib/perl5/5.6.0/i386-freebsd/CORE \
          -I/usr/local/perl/lib/5.6.0/i386-freebsd/CORE \
          -I/usr/libdata/perl/5.6.0/mach/CORE \
          -I/usr/local/lib/perl5/5.6.0/i386-freebsd/CORE
endif

ifeq ($(findstring linux,$(OSTYPE)), linux)
PERLINC = -I/usr/lib/perl5/i386-linux/CORE \
          -I/usr/lib/perl5/5.00503/i686-linux/CORE
endif

perl5ext: $(PROJECT).so

$(PROJECT).so: $(OBJ) $(PROJECT)_perl.o $(PROJECT)_wrap.o 
	$(CC) -o $@ -shared $^ -lperl $(LIBS1)
	
$(PROJECT)_wrap.c: $(PROJECT).i
	swig -perl5 $^

$(PROJECT)_wrap.o: $(PROJECT)_wrap.c
	$(CC) $(CFLAGS) -o $@ -c -O -g -Dbool=char -fpic $(PERLINC) $^

dist: clean
	cd .. && tar cvf $(DIST_NAME)-$(DIST_VERSION).tar $(DIST_NAME)
	gzip ../$(DIST_NAME)-$(DIST_VERSION).tar
