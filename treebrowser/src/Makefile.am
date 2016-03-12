include $(top_srcdir)/build/vars.build.mk
plugin = treebrowser

geanyplugins_LTLIBRARIES = treebrowser.la

treebrowser_la_SOURCES = treebrowser.c
treebrowser_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"TreeBrowser\"
treebrowser_la_CFLAGS = $(AM_CFLAGS) $(GIO_CFLAGS)
treebrowser_la_LIBADD = $(COMMONLIBS) $(GIO_LIBS)

AM_CPPCHECKFLAGS = --suppress='deallocDealloc:$(srcdir)/treebrowser.c'
AM_CPPCHECKFLAGS += --suppress='doubleFree:$(srcdir)/treebrowser.c'

include $(top_srcdir)/build/cppcheck.mk
