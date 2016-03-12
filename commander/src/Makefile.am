include $(top_srcdir)/build/vars.build.mk
plugin = commander

geanyplugins_LTLIBRARIES = commander.la


commander_la_SOURCES  = commander-plugin.c
commander_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -DG_LOG_DOMAIN=\"Commander\"
commander_la_CFLAGS   = $(AM_CFLAGS) \
                        $(COMMANDER_CFLAGS)
commander_la_LIBADD   = $(COMMONLIBS) \
                        $(COMMANDER_LIBS)


include $(top_srcdir)/build/cppcheck.mk
