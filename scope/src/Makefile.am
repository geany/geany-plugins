include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

geanyplugins_LTLIBRARIES = scope.la

plugin = scope

scope_la_SOURCES = \
	break.c \
	break.h \
	common.h \
	conterm.c \
	conterm.h \
	debug.c \
	debug.h \
	inspect.c \
	inspect.h \
	local.c \
	local.h \
	memory.c \
	memory.h \
	menu.c \
	menu.h \
	parse.c \
	parse.h \
	plugme.c \
	plugme.h \
	prefs.c \
	prefs.h \
	program.c \
	program.h \
	register.c \
	register.h \
	scope.c \
	scope.h \
	stack.c \
	stack.h \
	thread.c \
	thread.h \
	tooltip.c \
	tooltip.h \
	utils.c \
	utils.h \
	views.c \
	views.h \
	watch.c \
	watch.h \
	store.h \
	store/scptreedata.h \
	store/scptreedata.c \
	store/scptreestore.h \
	store/scptreestore.c

scope_la_LIBADD = $(COMMONLIBS) $(VTE_LIBS) $(PTY_LIBS) \
				  $(top_builddir)/utils/src/libgeanypluginutils.la

scope_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Scope\"
scope_la_CFLAGS = $(AM_CFLAGS) $(VTE_CFLAGS) \
	-DPLUGINHTMLDOCDIR=\"$(plugindocdir)/html\" \
	-Wno-shadow \
	-I$(top_srcdir)/utils/src

include $(top_srcdir)/build/cppcheck.mk
