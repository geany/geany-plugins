include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYDOC
geanyplugins_LTLIBRARIES = geanydoc.la
else
EXTRA_LTLIBRARIES = geanydoc.la
endif

geanydoc_la_SOURCES = \
	config.c \
	geanydoc.c \
	geanydoc.h

geanydoc_la_CFLAGS = $(AM_CFLAGS)
geanydoc_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
