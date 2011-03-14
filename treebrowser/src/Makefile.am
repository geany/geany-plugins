include $(top_srcdir)/build/vars.build.mk

if ENABLE_TREEBROWSER
geanyplugins_LTLIBRARIES = treebrowser.la
else
EXTRA_LTLIBRARIES = treebrowser.la
endif

treebrowser_la_SOURCES = treebrowser.c
treebrowser_la_CFLAGS = $(AM_CFLAGS) $(GIO_CFLAGS)
treebrowser_la_LIBADD = $(COMMONLIBS) $(GIO_LIBS)

include $(top_srcdir)/build/cppcheck.mk
