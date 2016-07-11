include $(top_srcdir)/build/vars.build.mk
plugin = autoclose

geanyplugins_LTLIBRARIES = autoclose.la

autoclose_la_SOURCES = autoclose.c
autoclose_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"AutoClose\"
autoclose_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
