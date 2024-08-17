BUILT_SOURCES =	\
	orgmode/parser/parser.h \
	orgmode/parser/parser.c \
	orgmode/parser/scanner.c

AM_CFLAGS = \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/orgmode/parser \
	-I @POPT_CFLAGS@ \
	-I @PISOCK_CFLAGS@
if ENABLE_FANALYZER
AM_CFLAGS += -fanalyzer -Wpedantic -Wall -Wextra
endif
AM_YFLAGS = -d -Wconflicts-sr -Wcounterexamples
AM_LDFLAGS = @POPT_LIBS@ \
	@PISOCK_LIBS@ \
	@ICONV_LIB@

bin_PROGRAMS = palm-sync-daemon

palm_sync_daemon_SOURCES = \
	include/log.h \
	log.c \
	include/umash.h \
	umash.c \
	include/helper.h \
	helper.c \
	include/palm.h \
	palm.c \
	include/pdb.h \
	pdb/pdb.c \
	include/pdb_memos.h \
	pdb/pdb_memos.c \
	orgmode/parser/orgmode_parser.h \
	orgmode/parser/parser.y \
	orgmode/parser/scanner.l \
	include/org_notes.h \
	orgmode/org_notes.c \
	include/sync.h \
	sync.c \
	palm-sync-daemon.c
CLEANFILES = $(BUILT_SOURCES)
