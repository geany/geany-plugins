include $(top_srcdir)/build/vars.build.mk
plugin = geanyprogress

geanyplugins_LTLIBRARIES = geanyprogress.la

geanyprogress_la_SOURCES  = geanyprogress.c
geanyprogress_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanyprogress_la_CFLAGS   = $(AM_CFLAGS)
geanyprogress_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
