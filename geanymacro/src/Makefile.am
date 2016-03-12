include $(top_srcdir)/build/vars.build.mk
plugin = geanymacro

geanyplugins_LTLIBRARIES = geanymacro.la

geanymacro_la_SOURCES = geanymacro.c
geanymacro_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyMacro\"
geanymacro_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
