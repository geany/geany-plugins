include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYLATEX
geanyplugins_LTLIBRARIES = geanylatex.la
else
EXTRA_LTLIBRARIES = geanylatex.la
endif

geanylatex_la_SOURCES = \
	bibtex.c \
	geanylatex.c \
	latexkeybindings.h \
	reftex.c \
	bibtex.h \
	geanylatex.h \
	latexstructure.c \
	reftex.h \
	bibtexlabels.c \
	latexencodings.c \
	latexstructure.h \
	templates.c \
	datatypes.h \
	latexencodings.h \
	latexutils.c \
	templates.h \
	formatpatterns.c \
	latexenvironments.c \
	latexutils.h \
	formatutils.c \
	latexenvironments.h \
	letters.c \
	formatutils.h \
	latexkeybindings.c \
	letters.h

geanylatex_la_LIBADD = $(COMMONLIBS)
