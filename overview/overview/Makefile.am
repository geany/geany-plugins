include $(top_srcdir)/build/vars.build.mk

plugin = overview

geanyplugins_LTLIBRARIES = overview.la

overview_la_SOURCES = \
	overviewcolor.c \
	overviewcolor.h \
	overviewplugin.c \
	overviewplugin.h \
	overviewprefs.c \
	overviewprefs.h \
	overviewprefspanel.c \
	overviewprefspanel.h \
	overviewscintilla.c \
	overviewscintilla.h \
	overviewui.c \
	overviewui.h

overview_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(srcdir)/.. \
	-DOVERVIEW_PREFS_UI_FILE=\""$(pkgdatadir)/overview/prefs.ui"\"

overview_la_LIBADD = \
	$(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
