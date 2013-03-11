include $(top_srcdir)/build/vars.build.mk
plugin = pohelper

geanyplugins_LTLIBRARIES = pohelper.la


pohelper_la_SOURCES  = gph-plugin.c
pohelper_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -DG_LOG_DOMAIN=\"PoHelper\"
pohelper_la_CFLAGS   = $(AM_CFLAGS)
pohelper_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
