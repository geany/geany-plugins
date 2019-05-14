include $(top_srcdir)/build/vars.build.mk
plugin = geanynumberedbookmarks

geanyplugins_LTLIBRARIES = geanynumberedbookmarks.la

geanynumberedbookmarks_la_SOURCES = geanynumberedbookmarks.c
geanynumberedbookmarks_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyNumberedBookmarks\"
geanynumberedbookmarks_la_CFLAGS = $(AM_CFLAGS) \
	-I$(top_srcdir)/utils/src
geanynumberedbookmarks_la_LIBADD = $(COMMONLIBS) \
	$(top_builddir)/utils/src/libgeanypluginutils.la
