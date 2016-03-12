include $(top_srcdir)/build/vars.build.mk
plugin = geanydoc

geanyplugins_LTLIBRARIES = geanydoc.la

geanydoc_la_SOURCES = \
	config.c \
	geanydoc.c \
	geanydoc.h

geanydoc_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyDoc\"
geanydoc_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
