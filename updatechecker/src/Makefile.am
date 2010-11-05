include $(top_srcdir)/build/vars.build.mk

if ENABLE_UPDATECHECKER
geanyplugins_LTLIBRARIES = updatechecker.la
else
EXTRA_LTLIBRARIES = updatechecker.la
endif
updatechecker_la_SOURCES = updatechecker.c
updatechecker_la_LIBADD = $(COMMONLIBS)
