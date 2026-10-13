include $(top_srcdir)/build/vars.build.mk
plugin = geanyenv

geanyplugins_LTLIBRARIES = geanyenv.la

geanyenv_la_SOURCES  = geanyenv.c
geanyenv_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanyenv_la_CFLAGS   = $(AM_CFLAGS)
geanyenv_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
