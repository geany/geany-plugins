include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = pretty-print.la

pretty_print_la_SOURCES = \
	PluginEntry.h \
	PluginEntry.c \
	PrettyPrinter.c \
	PrettyPrinter.h \
	ConfigUI.h \
	ConfigUI.c

pretty_print_la_CFLAGS = $(AM_CFLAGS) $(LIBXML_CFLAGS) -Wall
pretty_print_la_LIBADD = $(COMMONLIBS) $(LIBXML_LIBS)
