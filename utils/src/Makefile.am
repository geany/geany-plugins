include $(top_srcdir)/build/vars.build.mk

lib_LTLIBRARIES = libgeanypluginutils.la

libgeanypluginutils_la_SOURCES = \
	filelist.h \
	filelist.c

if SCOPE_USE_VTE291
libgeanypluginutils_la_SOURCES += \
	gp_vtecompat.h \
	gp_vtecompat.c
endif

libgeanypluginutils_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Utils\"
libgeanypluginutils_la_CFLAGS = $(AM_CFLAGS) $(VTE_CFLAGS)
libgeanypluginutils_la_LIBADD = $(COMMONLIBS) $(VTE_LIBS)
libgeanypluginutils_la_LDFLAGS = -no-undefined $(GP_LDFLAGS)
include $(top_srcdir)/build/cppcheck.mk

