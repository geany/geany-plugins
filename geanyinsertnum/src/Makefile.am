include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYINSERTNUM
geanyplugins_LTLIBRARIES = geanyinsertnum.la
else
EXTRA_LTLIBRARIES = geanyinsertnum.la
endif

geanyinsertnum_la_SOURCES = insertnum.c
geanyinsertnum_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
