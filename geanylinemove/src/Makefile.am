include $(top_srcdir)/build/vars.build.mk
plugin = geanylinemove

geanyplugins_LTLIBRARIES = geanylinemove.la

geanylinemove_la_SOURCES  = geanylinemove.c
geanylinemove_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyLineMove\"
geanylinemove_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
