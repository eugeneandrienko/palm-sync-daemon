AUTOMAKE_OPTIONS = no-dependencies
AM_CFLAGS = \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/orgmode/parser \
	-I @POPT_CFLAGS@ \
	-I @PISOCK_CFLAGS@
AM_YFLAGS = -d
AM_LDFLAGS = @POPT_LIBS@ \
	@PISOCK_LIBS@ \
	@ICONV_LIB@

TESTS = \
	helper_iconv_test.sh \
	helper_hash_test.sh \
	log_test.sh \
	pdb_test.sh \
	pdb_categories_test.sh \
	pdb_record_test.sh \
	pdb_memos_test.sh \
	pdb_memos_data_edit_test.sh \
	parser_test.sh \
	org_notes_test.sh \
	org_notes_write_test.sh \
	palm_sync_daemon_test.sh
check_PROGRAMS = \
	helper_iconv_test \
	helper_hash_test \
	log_test \
	pdb_test \
	pdb_categories_test \
	pdb_record_test \
	pdb_memos_test \
	pdb_memos_data_edit_test \
	parser_test \
	org_notes_test \
	org_notes_write_test \
	palm_sync_daemon_test
helper_iconv_test_SOURCES = \
	../src/log.c \
	../src/umash.c \
	../src/helper.c \
	helper_iconv_test.c
helper_hash_test_SOURCES = \
	../src/log.c \
	../src/umash.c \
	../src/helper.c \
	helper_hash_test.c
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
pdb_memos_test_SOURCES = \
	../src/umash.c \
	../src/helper.c \
	../src/log.c \
	../src/pdb/pdb.c \
	../src/pdb/pdb_memos.c \
	pdb_memos_test.c
pdb_memos_data_edit_test_SOURCES = \
	../src/umash.c \
	../src/helper.c \
	../src/log.c \
	../src/pdb/pdb.c \
	../src/pdb/pdb_memos.c \
	pdb_memos_data_edit_test.c
parser_test_SOURCES = \
	../src/log.c \
	../src/orgmode/parser/parser.y \
	../src/orgmode/parser/scanner.l \
	parser_test.c
org_notes_test_SOURCES = \
	../src/umash.c \
	../src/helper.c \
	../src/log.c \
	../src/orgmode/parser/parser.y \
	../src/orgmode/parser/scanner.l \
	../src/orgmode/org_notes.c \
	org_notes_test.c
org_notes_write_test_SOURCES = \
	../src/umash.c \
	../src/helper.c \
	../src/log.c \
	../src/orgmode/parser/parser.y \
	../src/orgmode/parser/scanner.l \
	../src/orgmode/org_notes.c \
	org_notes_write_test.c
palm_sync_daemon_test_SOURCES = \
	../src/palm-sync-daemon.c \
	../src/umash.c \
	../src/helper.c \
	../src/log.c \
	../src/palm.c \
	../src/pdb/pdb.c \
	../src/pdb/pdb_memos.c \
	../src/orgmode/parser/parser.y \
	../src/orgmode/parser/scanner.l \
	../src/orgmode/org_notes.c \
	../src/sync.c

EXTRA_DIST = $(TESTS)
