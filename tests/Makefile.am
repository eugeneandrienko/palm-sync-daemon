AM_CFLAGS = \
	-I$(top_srcdir)/include

TESTS = \
	log_test.sh \
	pdb_test.sh \
	pdb_categories_test.sh \
	pdb_record_test.sh \
	palm_sync_daemon_test.sh
check_PROGRAMS = \
	log_test \
	pdb_test \
	pdb_categories_test \
	pdb_record_test \
	palm_sync_daemon_test
log_test_SOURCES = \
	../src/log.c \
	log_test.c
pdb_test_SOURCES = \
	../src/log.c \
	../src/pdb/pdb.c \
	pdb_test.c
pdb_categories_test_SOURCES = \
	../src/log.c \
	../src/pdb/pdb.c \
	pdb_categories_test.c
pdb_record_test_SOURCES = \
	../src/log.c \
	../src/pdb/pdb.c \
	pdb_record_test.c
palm_sync_daemon_test_SOURCES = \
	../src/palm-sync-daemon.c \
	../src/log.c \
	../src/palm.c \
	../src/pdb/pdb.c

dist_bin_SCRIPTS = $(TESTS)
