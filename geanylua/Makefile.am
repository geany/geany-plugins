include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.auxfiles.mk

plugin = geanylua

EXTRA_DIST += util

geanyplugins_LTLIBRARIES = geanylua.la
geanyluadir = $(pkglibdir)/geanylua
geanylua_LTLIBRARIES = libgeanylua.la

geanylua_la_SOURCES = geanylua.c
libgeanylua_la_SOURCES = \
	glspi_app.c \
	glspi_dlg.c \
	glspi_doc.c \
	glspi_init.c \
	glspi_kfile.c \
	glspi_run.c \
	glspi_sci.c \
	gsdlg.c \
	gsdlg_lua.c \
	glspi.h \
	glspi_keycmd.h \
	glspi_sci.h \
	glspi_ver.h \
	glspi_compat.c \
	glspi_compat.h \
	gsdlg.h

geanylua_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(LUA_CFLAGS)

geanylua_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyLua\"
geanylua_la_LIBADD = \
	$(COMMONLIBS) \
	$(GMODULE_LIBS) \
	$(LUA_LIBS)

libgeanylua_la_CFLAGS = $(geanylua_la_CFLAGS)
libgeanylua_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyLua\"
libgeanylua_la_LIBADD = $(geanylua_la_LIBADD)

SUBDIRS = docs examples

include $(top_srcdir)/build/cppcheck.mk
