include $(top_srcdir)/build/vars.build.mk
plugin = geanylatex

geanyplugins_LTLIBRARIES = geanylatex.la

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

geanylatex_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyLaTeX\"
geanylatex_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
