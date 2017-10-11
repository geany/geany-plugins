include $(top_srcdir)/build/vars.build.mk

lib_LTLIBRARIES = libgeanypluginutils.la

libgeanypluginutils_la_SOURCES = \
	filelist.h \
	filelist.c

libgeanypluginutils_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Utils\"
libgeanypluginutils_la_CFLAGS = $(AM_CFLAGS)
libgeanypluginutils_la_LIBADD = $(COMMONLIBS)
libgeanypluginutils_la_LDFLAGS = $(GP_LDFLAGS)
include $(top_srcdir)/build/cppcheck.mk
