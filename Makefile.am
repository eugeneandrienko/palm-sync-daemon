bin_PROGRAMS = palm-sync-daemon
palm_sync_daemon_SOURCES = \
	include/log.h \
	src/log.c \
	include/palm.h \
	src/palm.c \
	include/pdb.h \
	src/pdb/pdb.c \
	src/palm-sync-daemon.c
AM_CFLAGS = \
	-I$(top_srcdir)/include

TESTS = \
	tests/log_test.sh \
	tests/pdb_test.sh \
	tests/pdb_categories_test.sh \
	tests/pdb_record_test.sh \
	tests/palm_sync_daemon_test.sh
check_PROGRAMS = \
	log_test \
	pdb_test \
	pdb_categories_test \
	pdb_record_test \
	palm_sync_daemon_test
log_test_SOURCES = \
	src/log.c \
	tests/log_test.c
pdb_test_SOURCES = \
	src/log.c \
	src/pdb/pdb.c \
	tests/pdb_test.c
pdb_categories_test_SOURCES = \
	src/log.c \
	src/pdb/pdb.c \
	tests/pdb_categories_test.c
pdb_record_test_SOURCES = \
	src/log.c \
	src/pdb/pdb.c \
	tests/pdb_record_test.c
palm_sync_daemon_test_SOURCES = \
	src/palm-sync-daemon.c \
	src/log.c \
	src/palm.c \
	src/pdb/pdb.c

dist_man_MANS = docs/palm-sync-daemon.1
dist_doc_DATA = \
	LICENSE \
	docs/technical_description.tex
dist_bin_SCRIPTS = $(TESTS)
