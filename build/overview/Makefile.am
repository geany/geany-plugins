include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk

plugin = overview

geanyplugins_LTLIBRARIES = overview.la

overview_la_SOURCES = \
	$(top_srcdir)/overview/overview/overviewcolor.c \
	$(top_srcdir)/overview/overview/overviewcolor.h \
	$(top_srcdir)/overview/overview/overviewplugin.c \
	$(top_srcdir)/overview/overview/overviewplugin.h \
	$(top_srcdir)/overview/overview/overviewprefs.c \
	$(top_srcdir)/overview/overview/overviewprefs.h \
	$(top_srcdir)/overview/overview/overviewprefspanel.c \
	$(top_srcdir)/overview/overview/overviewprefspanel.h \
	$(top_srcdir)/overview/overview/overviewscintilla.c \
	$(top_srcdir)/overview/overview/overviewscintilla.h \
	$(top_srcdir)/overview/overview/overviewui.c \
	$(top_srcdir)/overview/overview/overviewui.h

overview_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/overview \
	-DOVERVIEW_PREFS_UI_FILE=\""$(pkgdatadir)/overview/prefs.ui"\"

overview_la_LIBADD = \
	$(COMMONLIBS)

overviewdatadir = $(pkgdatadir)/overview
dist_overviewdata_DATA = $(top_srcdir)/overview/data/prefs.ui

include $(top_srcdir)/build/cppcheck.mk
