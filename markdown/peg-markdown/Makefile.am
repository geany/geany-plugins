SUBDIRS = peg-0.1.9

noinst_LTLIBRARIES = libpegmarkdown.la

libpegmarkdown_la_CPPFLAGS = $(GEANY_CFLAGS) 
libpegmarkdown_la_LIBADD = $(GEANY_LIBS)
libpegmarkdown_la_SOURCES = \
	markdown_lib.c \
	markdown_parser.c \
	markdown_output.c \
	markdown_peg.h \
	odf.c \
	parsing_functions.c \
	utility_functions.c

LEG = peg-0.1.9/leg

markdown_parser.c: markdown_parser.leg $(LEG)
	$(AM_V_GEN)$(LEG) -o $@ $<

EXTRA_DIST = markdown_parser.leg
