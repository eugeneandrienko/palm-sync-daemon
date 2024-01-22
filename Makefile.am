SUBDIRS = $(MAYBE_TEX) docs/man . tests
DIST_SUBDIRS = docs/tex docs/man tests

bin_PROGRAMS = palm-sync-daemon
palm_sync_daemon_SOURCES = \
	include/log.h \
	src/log.c \
	include/palm.h \
	src/palm.c \
	include/pdb.h \
	src/pdb/pdb.c \
	include/pdb_memos.h \
	src/pdb/pdb_memos.c \
	src/palm-sync-daemon.c
AM_CFLAGS = \
	-I$(top_srcdir)/include

dist_doc_DATA = LICENSE
