ACLOCAL_AMFLAGS = -I build/cache -I build -I build/bundled --install
AM_DISTCHECK_CONFIGURE_FLAGS = --with-geany-libdir='$${libdir}'

include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

plugin = tweaks_ui

geanyplugins_LTLIBRARIES = tweaks-ui.la

tweaks_ui_la_SOURCES = \
	src/tkui_main.cc \
	src/tkui_main.h \
	src/tkui_settings.cc \
	src/tkui_settings.h \
	src/tkui_auto_read_only.cc \
	src/tkui_auto_read_only.h \
	src/tkui_colortip.cc \
	src/tkui_colortip.h \
	src/tkui_column_markers.cc \
	src/tkui_column_markers.h \
	src/tkui_detect_filetype.cc \
	src/tkui_detect_filetype.h \
	src/tkui_hide_menubar.cc \
	src/tkui_hide_menubar.h \
	src/tkui_markword.cc \
	src/tkui_markword.h \
	src/tkui_sidebar_auto_position.cc \
	src/tkui_sidebar_auto_position.h \
	src/tkui_unchange_document.cc \
	src/tkui_unchange_document.h \
	src/tkui_window_geometry.cc \
	src/tkui_window_geometry.h \
	src/auxiliary.cc \
	src/auxiliary.h

tweaks_ui_la_CXXFLAGS = $(AM_CFLAGS) \
	-DG_LOG_DOMAIN=\""Tweaks-UI"\"

tweaks_ui_la_CFLAGS = $(AM_CFLAGS)
tweaks_ui_la_LIBADD = $(COMMONLIBS)
tweaks_ui_la_LDFLAGS = -module -avoid-version
