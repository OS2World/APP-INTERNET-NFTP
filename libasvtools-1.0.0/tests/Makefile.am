EXTRA_DIST = Makefile.os2

CLEANFILES = random.*

noinst_PROGRAMS = uniq$(EXEEXT) subtree$(EXEEXT) base64$(EXEEXT) \
  pathdcmp$(EXEEXT) finddesc$(EXEEXT) parse_date$(EXEEXT) \
  str_parseline$(EXEEXT) load_textfile$(EXEEXT) accumulate$(EXEEXT) \
  is_textfile$(EXEEXT) longprint$(EXEEXT) str_words$(EXEEXT) fsort$(EXEEXT) \
  merge$(EXEEXT) pretty$(EXEEXT) psort$(EXEEXT) xlf$(EXEEXT) \
  unixentry$(EXEEXT) connect$(EXEEXT) \
  str_replace$(EXEEXT) regex_test$(EXEEXT) \
  http_parse$(EXEEXT) binchars$(EXEEXT) mime$(EXEEXT) fprint_bits$(EXEEXT) \
  load_stdin$(EXEEXT) sortfile$(EXEEXT) linebuf$(EXEEXT) \
  check_list$(EXEEXT) http_getdate$(EXEEXT) \
  fnmatch1$(EXEEXT) snprintf1$(EXEEXT) clock1$(EXEEXT) bracket$(EXEEXT) \
  ranges$(EXEEXT) vby$(EXEEXT) pretty_date$(EXEEXT) math_routines$(EXEEXT)

LDADD = ../src/libasvtools.la

test: all
	@ for p in $(noinst_PROGRAMS); do \
		echo "....$$p" ;\
		$$p$(EXEEXT) ;\
	done
