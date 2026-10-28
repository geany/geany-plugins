include $(top_srcdir)/build/vars.build.mk
plugin = geanycli

geanyplugins_LTLIBRARIES = geanycli.la

geanycli_la_SOURCES  = geanycli.c geanycli.h
geanycli_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanycli_la_CFLAGS   = $(AM_CFLAGS) $(VTE_CFLAGS)
geanycli_la_LIBADD   = $(COMMONLIBS) $(VTE_LIBS)

include $(top_srcdir)/build/cppcheck.mk
