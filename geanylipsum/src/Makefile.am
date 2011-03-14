include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYLIPSUM
geanyplugins_LTLIBRARIES = geanylipsum.la
else
EXTRA_LTLIBRARIES = geanylipsum.la
endif

geanylipsum_la_SOURCES = geanylipsum.c
geanylipsum_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
