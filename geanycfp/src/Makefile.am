include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYCFP
geanyplugins_LTLIBRARIES = geanycfp.la
else
EXTRA_LTLIBRARIES = geanycfp.la
endif

geanycfp_la_SOURCES = geanycfp.c
geanycfp_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
