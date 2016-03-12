include $(top_srcdir)/build/vars.build.mk
plugin = automark

geanyplugins_LTLIBRARIES = automark.la

automark_la_SOURCES = automark.c
automark_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"AutoMark\"
automark_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
