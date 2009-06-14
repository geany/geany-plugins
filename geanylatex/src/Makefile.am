include $(top_srcdir)/build/vars.mk

geanyplugins_LTLIBRARIES = geanylatex.la

geanylatex_la_SOURCES = \
	letters.h \
	latexutils.h \
	formatutils.h \
	latexencodings.h \
	latexkeybindings.h \
	datatypes.h \
	geanylatex.h \
	reftex.h \
	bibtex.h \
	latexenvironments.h \
	geanylatex.c \
	latexencodings.c \
	bibtex.c \
	bibtexlabels.c \
	latexutils.c \
	formatpatterns.c \
	latexenvironments.c \
	letters.c \
	latexkeybindings.c \
	reftex.c \
	formatutils.c

geanylatex_la_LIBADD = $(COMMONLIBS)
geanylatex_la_LDFLAGS = -module -avoid-version
