include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk

plugin = overview

geanyplugins_LTLIBRARIES = overview.la

overview_la_SOURCES = \
	../../overview/overview/overviewcolor.c \
	../../overview/overview/overviewcolor.h \
	../../overview/overview/overviewplugin.c \
	../../overview/overview/overviewplugin.h \
	../../overview/overview/overviewprefs.c \
	../../overview/overview/overviewprefs.h \
	../../overview/overview/overviewprefspanel.c \
	../../overview/overview/overviewprefspanel.h \
	../../overview/overview/overviewscintilla.c \
	../../overview/overview/overviewscintilla.h \
	../../overview/overview/overviewui.c \
	../../overview/overview/overviewui.h

overview_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/overview \
	-DOVERVIEW_PREFS_UI_FILE=\""$(pkgdatadir)/overview/prefs.ui"\"

overview_la_LIBADD = \
	$(COMMONLIBS)

overviewdatadir = $(pkgdatadir)/overview
dist_overviewdata_DATA = $(top_srcdir)/overview/data/prefs.ui

include $(top_srcdir)/build/cppcheck.mk
