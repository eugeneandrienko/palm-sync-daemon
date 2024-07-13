if ENABLE_TEX_DOC
if HAVE_PDFLATEX
all-local:
	if [ "$(srcdir)" != "$(builddir)" ]; then \
		chmod -R u+w $(builddir)/images 2>/dev/null || /bin/true; \
		cp -rv $(srcdir)/images .; \
		chmod u+w $(builddir)/*.tex 2>/dev/null || /bin/true; \
		cp -rv $(srcdir)/*.tex .; \
		chmod u+w $(builddir)/*.bib 2>/dev/null || /bin/true; \
		cp -rv $(srcdir)/*.bib .; \
	fi
	latexmk -pdf -silent

clean-local:
	latexmk -C

distclean-local:
	if [ "$(srcdir)" != "$(builddir)" ]; then \
		chmod -R u+w $(builddir)/images 2>/dev/null || /bin/true; \
		chmod u+w $(builddir)/*.tex 2>/dev/null || /bin/true; \
		chmod u+w $(builddir)/*.bib 2>/dev/null || /bin/true; \
		chmod u+w $(builddir)/*.bbl 2>/dev/null || /bin/true; \
		chmod u+w $(builddir)/*.run.xml 2>/dev/null || /bin/true; \
		rm -rfv $(builddir)/images \
			$(builddir)/*.tex \
			$(builddir)/*.bib \
			$(builddir)/*.bbl \
			$(builddir)/*.lol \
			$(builddir)/*.run.xml; \
	else \
		rm -rfv $(srcdir)/*.bbl \
			$(srcdir)/*.run.xml; \
	fi

EXTRA_DIST = \
	$(srcdir)/bibliography.bib \
	$(srcdir)/images/* \
	$(srcdir)/technical_description.tex \
	$(srcdir)/technical_description.pdf
else
EXTRA_DIST = \
	$(srcdir)/bibliography.bib \
	$(srcdir)/images/* \
	$(srcdir)/technical_description.tex
endif
endif
