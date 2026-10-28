include $(top_srcdir)/build/vars.build.mk
plugin = geanycontrol

geanyplugins_LTLIBRARIES = geanycontrol.la

geanycontrol_la_SOURCES  = geanycontrol.c
geanycontrol_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanycontrol_la_CFLAGS   = $(AM_CFLAGS)
geanycontrol_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
