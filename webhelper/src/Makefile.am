include $(top_srcdir)/build/vars.build.mk
plugin = webhelper

geanyplugins_LTLIBRARIES = webhelper.la


GLIB_MKENUMS = @GLIB_MKENUMS@


sources         = gwh-browser.c \
                  gwh-keybindings.c \
                  gwh-plugin.c \
                  gwh-settings.c \
                  gwh-utils.c
headers         = gwh-browser.h \
                  gwh-keybindings.h \
                  gwh-plugin.h \
                  gwh-settings.h \
                  gwh-utils.h
autogen_sources = gwh-enum-types.c
autogen_headers = gwh-enum-types.h

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
                        $(WEBHELPER_CFLAGS)
webhelper_la_LIBADD   = $(COMMONLIBS) \
                        $(WEBHELPER_LIBS) \
                        -lm

# These are generated in $(srcdir) because they are part of the distribution,
# and should anyway only be regenerated if the .tpl changes, which is a
# developer thing ,so if should be harmless.
$(srcdir)/gwh-enum-types.c: $(srcdir)/gwh-enum-types.c.tpl gwh-enum-types.h $(headers)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(headers:%=$(srcdir)/%) > $@
$(srcdir)/gwh-enum-types.h: $(srcdir)/gwh-enum-types.h.tpl $(headers)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(headers:%=$(srcdir)/%) > $@

include $(top_srcdir)/build/cppcheck.mk
