include $(top_srcdir)/build/vars.build.mk

plugin = markdown

geanyplugins_LTLIBRARIES = markdown.la

markdown_la_SOURCES = \
	conf.c \
	plugin.c \
	viewer.c \
	markdown-gtk-compat.c

noinst_HEADERS = \
	conf.h \
	viewer.h \
	markdown-gtk-compat.h

markdown_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MARKDOWN_CFLAGS) \
	-DMARKDOWN_DATA_DIR=\"$(pkgdatadir)/markdown\" \
	-DMARKDOWN_DOC_DIR=\"$(docdir)/markdown\" \
	-DMARKDOWN_HELP_FILE=\"$(docdir)/markdown/html/help.html\"

markdown_la_LIBADD = \
	$(MARKDOWN_LIBS)

if MARKDOWN_PEG_MARKDOWN
markdown_la_CFLAGS += -DFULL_PRICE -I$(top_srcdir)/markdown/peg-markdown
markdown_la_LIBADD += $(top_builddir)/markdown/peg-markdown/libpegmarkdown.la
else
markdown_la_CFLAGS += $(LIBMARKDOWN_CFLAGS)
markdown_la_LIBADD += $(LIBMARKDOWN_LIBS)
endif

include $(top_srcdir)/build/cppcheck.mk
