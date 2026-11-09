include $(top_srcdir)/build/vars.build.mk
plugin = geanyservers

geanyplugins_LTLIBRARIES = geanyservers.la

geanyservers_la_SOURCES  = geanyservers.c
geanyservers_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanyservers_la_CFLAGS   = $(AM_CFLAGS)
geanyservers_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
