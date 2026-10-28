include $(top_srcdir)/build/vars.build.mk
plugin = linuxrectselect

geanyplugins_LTLIBRARIES = linuxrectselect.la

linuxrectselect_la_SOURCES  = linuxrectselect.c
linuxrectselect_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
linuxrectselect_la_CFLAGS   = $(AM_CFLAGS)
linuxrectselect_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
