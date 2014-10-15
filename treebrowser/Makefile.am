include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = treebrowser

geanyplugins_LTLIBRARIES = treebrowser.la

treebrowser_la_SOURCES = src/treebrowser.c
treebrowser_la_CFLAGS = $(AM_CFLAGS) $(GIO_CFLAGS)
treebrowser_la_LIBADD = $(COMMONLIBS) $(GIO_LIBS)

AM_CPPCHECKFLAGS = --suppress='deallocDealloc:$(srcdir)/src/treebrowser.c'
AM_CPPCHECKFLAGS += --suppress='doubleFree:$(srcdir)/src/treebrowser.c'

include $(top_srcdir)/build/cppcheck.mk
