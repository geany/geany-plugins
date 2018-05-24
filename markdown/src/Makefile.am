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
	-DMARKDOWN_DOC_DIR=PLUGINDOCDIR \
	-DMARKDOWN_HELP_FILE=\"$(GP_DOCDIR)/$(plugin)/html/help.html\"

markdown_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Markdown\"
markdown_la_LIBADD = $(COMMONLIBS) $(MARKDOWN_LIBS)

if MD_BUILTIN_CMARK
markdown_la_CPPFLAGS += -I$(top_srcdir)/markdown/cmark
markdown_la_LIBADD += $(top_builddir)/markdown/cmark/libcmark.la
else
markdown_la_CFLAGS += $(CMARK_CFLAGS)
markdown_la_LIBADD += $(CMARK_LIBS)
endif

include $(top_srcdir)/build/cppcheck.mk
