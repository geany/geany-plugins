include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = pretty-print.la

pretty_print_la_SOURCES = \
	Plugin.c \
	PrettyPrinter.c \
	PrettyPrinter.h

pretty_print_la_CFLAGS = $(AM_CFLAGS) $(LIBXML_CFLAGS)
pretty_print_la_LIBADD = $(COMMONLIBS) $(LIBXML_LIBS)
