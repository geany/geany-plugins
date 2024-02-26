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
	
if GP_GTK3
DEVHELP_LOCAL_CFLAGS =
DEVHELP_LOCAL_LDFLAGS =
else !GP_GTK3
DEVHELP_LOCAL_CFLAGS = -I$(top_srcdir)/devhelp
DEVHELP_LOCAL_LDFLAGS = $(top_builddir)/devhelp/devhelp/libdevhelp-2.la
endif !GP_GTK3


devhelp_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(DEVHELP_CFLAGS) \
	-DDHPLUG_DATA_DIR=\"$(plugindatadir)\" \
	-DHAVE_BOOK_MANAGER=1\
	$(DEVHELP_LOCAL_LDFLAGS)

devhelp_la_LIBADD = \
	$(COMMONLIBS) \
	$(DEVHELP_LIBS) \
	$(DEVHELP_LOCAL_LDFLAGS)

include $(top_srcdir)/build/cppcheck.mk
