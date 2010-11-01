include $(top_srcdir)/build/vars.build.mk
plugin = webhelper

if ENABLE_WEBHELPER
geanyplugins_LTLIBRARIES = webhelper.la
else
EXTRA_LTLIBRARIES = webhelper.la
endif


GLIB_MKENUMS = @GLIB_MKENUMS@


sources         = gwh-browser.c \
                  gwh-plugin.c \
                  gwh-settings.c \
                  gwh-utils.c
headers         = gwh-browser.h \
                  gwh-settings.h \
                  gwh-utils.h
autogen_sources = gwh-enum-types.c
autogen_headers = gwh-enum-types.h

CLEANFILES      = $(autogen_sources) \
                  $(autogen_headers)

EXTRA_DIST      = gwh-enum-types.c.tpl \
                  gwh-enum-types.h.tpl


webhelper_la_SOURCES  = $(autogen_sources) \
                        $(autogen_headers) \
                        $(sources) \
                        $(headers)
webhelper_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -I$(srcdir) -I$(builddir) \
                        -DG_LOG_DOMAIN=\"WebHelper\"
webhelper_la_CFLAGS   = $(AM_CFLAGS) \
                        $(GLIB_CFLAGS) \
                        $(GIO_CFLAGS) \
                        $(GTK_CFLAGS) \
                        $(GDK_PIXBUF_CFLAGS) \
                        $(WEBKIT_CFLAGS)
webhelper_la_LIBADD   = $(COMMONLIBS) \
                        $(GLIB_LIBS) \
                        $(GIO_LIBS) \
                        $(GTK_LIBS) \
                        $(GDK_PIXBUF_LIBS) \
                        $(WEBKIT_LIBS)

gwh-enum-types.c: $(srcdir)/gwh-enum-types.c.tpl gwh-enum-types.h $(headers) Makefile
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(headers:%=$(srcdir)/%) > $@
gwh-enum-types.h: $(srcdir)/gwh-enum-types.h.tpl $(headers) Makefile
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(headers:%=$(srcdir)/%) > $@
