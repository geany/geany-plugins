include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = utils.la

utils_la_SOURCES = \
	filelist.h \
	filelist.c

utils_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Utils\"
utils_la_CFLAGS = $(AM_CFLAGS)
utils_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
