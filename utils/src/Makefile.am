include $(top_srcdir)/build/vars.build.mk

lib_LTLIBRARIES = libgeanypluginutils.la

libgeanypluginutils_la_SOURCES = \
	filelist.h \
	filelist.c \
	gp_gtkcompat.h

if ENABLE_UTILSLIB_VTECOMPAT
libgeanypluginutils_la_SOURCES += \
	gp_vtecompat.h \
	gp_vtecompat.c
endif

libgeanypluginutils_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Utils\"
libgeanypluginutils_la_CFLAGS = $(AM_CFLAGS) $(UTILSLIB_CFLAGS)
libgeanypluginutils_la_LIBADD = $(COMMONLIBS) $(UTILSLIB_LIBS)
libgeanypluginutils_la_LDFLAGS = -no-undefined $(GP_LDFLAGS)
include $(top_srcdir)/build/cppcheck.mk
