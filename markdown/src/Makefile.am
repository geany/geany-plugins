include $(top_srcdir)/build/vars.build.mk

plugin = markdown

geanyplugins_LTLIBRARIES = markdown.la

markdown_la_SOURCES = \
	conf.c \
	plugin.c \
	viewer.c

noinst_HEADERS = \
	conf.h \
	viewer.h

markdown_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MARKDOWN_CFLAGS) \
	-DMARKDOWN_DATA_DIR=\"$(pkgdatadir)/markdown\" \
	-DMARKDOWN_DOC_DIR=\"$(docdir)/markdown\" \
	-DMARKDOWN_HELP_FILE=\"$(docdir)/markdown/html/help.html\" \
	-I$(top_srcdir)/markdown/discount

markdown_la_LIBADD = \
	$(MARKDOWN_LIBS) \
	$(top_builddir)/markdown/discount/libdiscount.la

include $(top_srcdir)/build/cppcheck.mk
