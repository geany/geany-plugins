include $(top_srcdir)/build/vars.build.mk
plugin = locate

geanyplugins_LTLIBRARIES = locate.la

locate_la_SOURCES  = locate.c
locate_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
locate_la_CFLAGS   = $(AM_CFLAGS)
locate_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
