include $(top_srcdir)/build/vars.build.mk

plugin = devhelp

geanyplugins_LTLIBRARIES = devhelp.la

devhelp_la_SOURCES = \
	dhp-manpages.c \
	dhp-object.c \
	dhp-plugin.c

EXTRA_DIST = \
	dhp-settings.c

noinst_HEADERS = \
	dhp.h \
	dhp-plugin.h
	
devhelp_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(DEVHELP_CFLAGS) \
	-DDHPLUG_DATA_DIR=\"$(plugindatadir)\" \
	-DHAVE_BOOK_MANAGER=1

devhelp_la_LIBADD = \
	$(COMMONLIBS) \
	$(DEVHELP_LIBS) 
	

include $(top_srcdir)/build/cppcheck.mk
