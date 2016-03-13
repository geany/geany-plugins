include $(top_srcdir)/build/vars.build.mk
plugin = geanyminiscript

geanyplugins_LTLIBRARIES = geanyminiscript.la

geanyminiscript_la_SOURCES = gms.c gms.h gms_gui.c gms_gui.h gms_debug.h
geanyminiscript_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyMiniScript\"
geanyminiscript_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
