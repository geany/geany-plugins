# don't install docs if disabled
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.auxfiles.mk
plugin = geanylua
else
include $(top_srcdir)/build/vars.docs.mk
EXTRA_DIST = $(AUXFILES)
endif
EXTRA_DIST += util
include $(top_srcdir)/build/vars.build.mk


# don't build if disabled
if ENABLE_GEANYLUA
geanyplugins_LTLIBRARIES = geanylua.la
geanyluadir = $(pkglibdir)/geanylua
geanylua_LTLIBRARIES = libgeanylua.la
else
EXTRA_LTLIBRARIES = geanylua.la libgeanylua.la
endif

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
	gsdlg.h

geanylua_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LUA_CFLAGS)

geanylua_la_LIBADD = \
	$(COMMONLIBS) \
	$(LUA_LIBS)

libgeanylua_la_CFLAGS = $(geanylua_la_CFLAGS)
libgeanylua_la_LIBADD = $(geanylua_la_LIBADD)

SUBDIRS = docs examples

include $(top_srcdir)/build/cppcheck.mk
