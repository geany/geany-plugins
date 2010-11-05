include $(top_srcdir)/build/vars.build.mk

if ENABLE_UPDATECHECKER
geanyplugins_LTLIBRARIES = updatechecker.la
else
EXTRA_LTLIBRARIES = updatechecker.la
endif
geanylipsum_la_SOURCES = updatechecker.c
geanylipsum_la_LIBADD = $(COMMONLIBS)
