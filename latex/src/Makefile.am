include $(top_srcdir)/build/vars.build.mk
plugin = latex

geanyplugins_LTLIBRARIES = latex.la

latex_la_SOURCES = \
	bibtex.c \
	latex.c \
	latexkeybindings.h \
	reftex.c \
	bibtex.h \
	latex.h \
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

latex_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"LaTeX\"
latex_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
