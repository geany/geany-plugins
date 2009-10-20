include $(top_srcdir)/build/vars.build.mk

if ENABLE_PRETTYPRINTER
geanyplugins_LTLIBRARIES = pretty-print.la
else
EXTRA_LTLIBRARIES = pretty-print.la
endif

pretty_print_la_SOURCES = \
	PluginEntry.h \
	PluginEntry.c \
	PrettyPrinter.c \
	PrettyPrinter.h \
	ConfigUI.h \
	ConfigUI.c

pretty_print_la_CFLAGS = $(AM_CFLAGS) $(LIBXML_CFLAGS)
pretty_print_la_LIBADD = $(COMMONLIBS) $(LIBXML_LIBS)
