include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk

plugin = geanylua

geanyplugins_LTLIBRARIES = geanylua.la

geanylua_la_SOURCES = \
	geanylua.c \
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
	gsdlg.h

geanylua_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LUA_CFLAGS)

geanylua_la_LIBADD = \
	$(COMMONLIBS) \
	$(LUA_LIBS)

SUBDIRS = docs examples
EXTRA_DIST = util
