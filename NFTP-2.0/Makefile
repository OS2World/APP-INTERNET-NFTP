
VERSION=2.00.b10

ifndef PLATFORM

ifdef RUNWORKPLACE
PLATFORM = i386-os2
else
ifdef windir
PLATFORM = i386-win32
else
PLATFORM = `sh ./Makefiles/what-platform`
endif
endif

endif

all:

%:
	@echo Making [$*] of version [$(VERSION)] for [$(PLATFORM)]
	$(MAKE) -f Makefiles/common PLATFORM=$(PLATFORM) VERSION=$(VERSION) $*
