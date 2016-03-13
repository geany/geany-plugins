include $(top_srcdir)/build/vars.build.mk
plugin = shiftcolumn

geanyplugins_LTLIBRARIES = shiftcolumn.la

shiftcolumn_la_SOURCES = shiftcolumn.c
shiftcolumn_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"ShiftColumn\"
shiftcolumn_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
