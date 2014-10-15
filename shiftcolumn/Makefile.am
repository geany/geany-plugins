include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = shiftcolumn

geanyplugins_LTLIBRARIES = shiftcolumn.la

shiftcolumn_la_SOURCES = src/shiftcolumn.c
shiftcolumn_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
