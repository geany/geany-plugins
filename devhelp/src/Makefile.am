include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

plugin = devhelp

if ENABLE_DEVHELP
geanyplugins_LTLIBRARIES = devhelp.la
else
EXTRA_LTLIBRARIES = devhelp.la
endif

devhelp_la_SOURCES = \
	dhp-codesearch.c \
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
	-DDHPLUG_DATA_DIR=\"$(plugindatadir)\"

devhelp_la_LIBADD = $(DEVHELP_LIBS)

include $(top_srcdir)/build/cppcheck.mk
