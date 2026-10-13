include $(top_srcdir)/build/vars.build.mk
plugin = geanyagent

geanyplugins_LTLIBRARIES = geanyagent.la

geanyagent_la_SOURCES  = geanyagent.c
geanyagent_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanyagent_la_CFLAGS   = $(AM_CFLAGS) $(VTE_CFLAGS)
geanyagent_la_LIBADD   = $(COMMONLIBS) $(VTE_LIBS)

include $(top_srcdir)/build/cppcheck.mk
