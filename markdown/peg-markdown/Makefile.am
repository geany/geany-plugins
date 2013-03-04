SUBDIRS = peg-0.1.9

noinst_LTLIBRARIES = libpegmarkdown.la

libpegmarkdown_la_CPPFLAGS = $(GEANY_CFLAGS) 
libpegmarkdown_la_LIBADD = $(GEANY_LIBS)
libpegmarkdown_la_SOURCES = \
	markdown_lib.c \
	markdown_lib.h \
	markdown_output.c \
	markdown_parser.c \
	markdown_peg.h \
	odf.c \
	odf.h \
	parsing_functions.c \
	parsing_functions.h \
	utility_functions.c \
	utility_functions.h

LEG = peg-0.1.9/leg

$(LEG):
	$(MAKE) -C peg-0.1.9 leg

markdown_parser.c: markdown_parser.leg $(LEG)
	$(AM_V_GEN)$(LEG) -o $@ $<

CLEANFILES = markdown_parser.c
EXTRA_DIST = markdown_parser.leg
