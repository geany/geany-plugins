include $(top_srcdir)/build/vars.build.mk
plugin = pretty-printer

geanyplugins_LTLIBRARIES = pretty-printer.la

pretty_printer_la_SOURCES = \
	PluginEntry.h \
	PluginEntry.c \
	PrettyPrinter.c \
	PrettyPrinter.h \
	ConfigUI.h \
	ConfigUI.c

pretty_printer_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"PrettyPrinter\"
pretty_printer_la_CFLAGS = $(AM_CFLAGS) $(LIBXML_CFLAGS) -DHAVE_GLIB -DHAVE_LIBXML
pretty_printer_la_LIBADD = $(COMMONLIBS) $(LIBXML_LIBS)

include $(top_srcdir)/build/cppcheck.mk
