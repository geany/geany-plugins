include $(top_srcdir)/build/vars.build.mk

if ENABLE_PRETTY_PRINTER
geanyplugins_LTLIBRARIES = pretty-printer.la
else
EXTRA_LTLIBRARIES = pretty-printer.la
endif

pretty_printer_la_SOURCES = \
	PluginEntry.h \
	PluginEntry.c \
	PrettyPrinter.c \
	PrettyPrinter.h \
	ConfigUI.h \
	ConfigUI.c

pretty_printer_la_CFLAGS = $(AM_CFLAGS) $(LIBXML_CFLAGS) -DHAVE_GLIB -DHAVE_LIBXML
pretty_printer_la_LIBADD = $(COMMONLIBS) $(LIBXML_LIBS)

include $(top_srcdir)/build/cppcheck.mk
