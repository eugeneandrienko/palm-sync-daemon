SUBDIRS = \
	$(MAYBE_TEX) \
	$(MAYBE_DOXYGEN) \
	docs/man \
	src \
	tests
DIST_SUBDIRS = \
	docs/tex \
	docs/man \
	docs/doxygen \
	src \
	tests

dist_doc_DATA = LICENSE
EXTRA_DIST = $(srcdir)/scripts/*

all-local:
	if [ "$(srcdir)" == "$(builddir)" ]; then \
		$(srcdir)/scripts/static_analysis.sh $(MAYBE_CPPCHECK); \
		etags --version > /dev/null; \
		if [ "$$?" -eq "0" ]; then \
			find src/ -type f -regex '.*\.[ch]' | xargs etags --language=c; \
		fi \
	fi

clean-local:
	if [ "$(srcdir)" == "$(builddir)" ]; then \
		rm -f $(top_srcdir)/static_analysis_report.txt; \
	fi
