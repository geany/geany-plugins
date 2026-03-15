include $(top_srcdir)/build/vars.build.mk
plugin = commander

geanyplugins_LTLIBRARIES = commander.la


commander_la_SOURCES  = commander-plugin.c
commander_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -DG_LOG_DOMAIN=\"Commander\" \
                        -I$(top_srcdir)/utils/src
commander_la_CFLAGS   = $(AM_CFLAGS) \
                        $(COMMANDER_CFLAGS)
commander_la_LIBADD   = $(COMMONLIBS) \
                        $(COMMANDER_LIBS) \
                        -lutil $(top_builddir)/utils/src/libgeanypluginutils.la


include $(top_srcdir)/build/cppcheck.mk
