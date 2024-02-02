SUBDIRS = \
	$(MAYBE_TEX) \
	$(MAYBE_DOXYGEN) \
	docs/man \
	. \
	tests
DIST_SUBDIRS = \
	docs/tex \
	docs/man \
	docs/doxygen \
	tests

bin_PROGRAMS = palm-sync-daemon

BUILT_SOURCES =	\
	src/orgmode/parser/parser.h \
	src/orgmode/parser/parser.c \
	src/orgmode/parser/scanner.c

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/orgmode/parser
AM_YFLAGS = -d

palm_sync_daemon_SOURCES = \
	include/log.h \
	src/log.c \
	include/palm.h \
	src/palm.c \
	include/pdb.h \
	src/pdb/pdb.c \
	include/pdb_memos.h \
	src/pdb/pdb_memos.c \
	src/orgmode/parser/orgmode_parser.h \
	src/orgmode/parser/parser.y \
	src/orgmode/parser/scanner.l \
	include/org_notes.h \
	src/orgmode/org_notes.c \
	src/palm-sync-daemon.c
CLEANFILES = $(BUILT_SOURCES)

dist_doc_DATA = LICENSE
EXTRA_DIST = $(srcdir)/scripts/*

all-local:
	if [ "$(srcdir)" == "$(builddir)" ]; then \
		$(srcdir)/scripts/static_analysis.sh $(MAYBE_CPPCHECK); \
	fi

clean-local:
	if [ "$(srcdir)" == "$(builddir)" ]; then \
		rm -f $(top_srcdir)/static_analysis_report.txt; \
	fi
