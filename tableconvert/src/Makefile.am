include $(top_srcdir)/build/vars.build.mk

if ENABLE_TABLECONVERT
geanyplugins_LTLIBRARIES = tableconvert.la
else
EXTRA_LTLIBRARIES = tableconvert.la
endif

tableconvert_la_SOURCES = tableconvert.c
tableconvert_la_LIBADD = $(COMMONLIBS)
