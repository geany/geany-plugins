include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYMACRO
geanyplugins_LTLIBRARIES = geanymacro.la
else
EXTRA_LTLIBRARIES = geanymacro.la
endif

geanymacro_la_SOURCES = geanymacro.c
geanymacro_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
