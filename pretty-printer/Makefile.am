# include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = pretty-printer

geanyplugins_LTLIBRARIES = pretty-printer.la

pretty_printer_la_SOURCES = \
	src/PluginEntry.h \
	src/PluginEntry.c \
	src/PrettyPrinter.c \
	src/PrettyPrinter.h \
	src/ConfigUI.h \
	src/ConfigUI.c

pretty_printer_la_CFLAGS = $(AM_CFLAGS) $(LIBXML_CFLAGS) -DHAVE_GLIB -DHAVE_LIBXML
pretty_printer_la_LIBADD = $(COMMONLIBS) $(LIBXML_LIBS)

include $(top_srcdir)/build/cppcheck.mk
