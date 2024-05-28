include $(top_srcdir)/build/vars.build.mk
plugin = treebrowser

geanyplugins_LTLIBRARIES = treebrowser.la

treebrowser_la_SOURCES = treebrowser.c
treebrowser_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"TreeBrowser\"
treebrowser_la_CFLAGS = $(AM_CFLAGS) $(GIO_CFLAGS) \
	-I$(top_srcdir)/utils/src
treebrowser_la_LIBADD = $(COMMONLIBS) $(GIO_LIBS) \
	$(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
