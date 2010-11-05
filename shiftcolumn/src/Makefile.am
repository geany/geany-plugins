include $(top_srcdir)/build/vars.build.mk

if ENABLE_SHIFTCOLUMN
geanyplugins_LTLIBRARIES = shiftcolumn.la
else
EXTRA_LTLIBRARIES = shiftcolumn.la
endif

shiftcolumn_la_SOURCES = shiftcolumn.c
shiftcolumn_la_LIBADD = $(COMMONLIBS)
