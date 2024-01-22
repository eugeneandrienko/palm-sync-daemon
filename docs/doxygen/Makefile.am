if HAVE_DOXYGEN
all-local: Doxyfile
	doxygen -q Doxyfile

distclean-local:
	rm -rf $(builddir)/html/

EXTRA_DIST = \
	$(srcdir)/html/*
endif
