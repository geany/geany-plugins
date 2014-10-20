include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = webhelper

AUXFILES = TODO

geanyplugins_LTLIBRARIES = webhelper.la


GLIB_MKENUMS = @GLIB_MKENUMS@


sources         = src/gwh-browser.c \
                  src/gwh-keybindings.c \
                  src/gwh-plugin.c \
                  src/gwh-settings.c \
                  src/gwh-utils.c
headers         = src/gwh-browser.h \
                  src/gwh-keybindings.h \
                  src/gwh-plugin.h \
                  src/gwh-settings.h \
                  src/gwh-utils.h
autogen_sources = src/gwh-enum-types.c
autogen_headers = src/gwh-enum-types.h

EXTRA_DIST     += src/gwh-enum-types.c.tpl \
                  src/gwh-enum-types.h.tpl


webhelper_la_SOURCES  = $(autogen_sources) \
                        $(autogen_headers) \
                        $(sources) \
                        $(headers)
webhelper_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -I$(srcdir)/src -I$(builddir)/src \
                        -DG_LOG_DOMAIN=\"WebHelper\"
webhelper_la_CFLAGS   = $(AM_CFLAGS) \
                        $(WEBHELPER_CFLAGS)
webhelper_la_LIBADD   = $(COMMONLIBS) \
                        $(WEBHELPER_LIBS)

# These are generated in $(srcdir) because they are part of the distribution,
# and should anyway only be regenerated if the .tpl changes, which is a
# developer thing ,so if should be harmless.
$(srcdir)/src/gwh-enum-types.c: $(srcdir)/src/gwh-enum-types.c.tpl src/gwh-enum-types.h $(headers)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(headers:%=$(srcdir)/%) > $@
$(srcdir)/src/gwh-enum-types.h: $(srcdir)/src/gwh-enum-types.h.tpl $(headers)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(headers:%=$(srcdir)/%) > $@

include $(top_srcdir)/build/cppcheck.mk
