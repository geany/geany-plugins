include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

SUBDIRS = xpm

if ENABLE_DEBUGGER
geanyplugins_LTLIBRARIES = debugger.la
else
EXTRA_LTLIBRARIES = debugger.la
endif

plugin = debugger

debugger_la_SOURCES = \
	bptree.c     \
	bptree.h     \
	breakpoint.c     \
	breakpoint.h     \
	breakpoints.c     \
	breakpoints.h     \
	btnpanel.c     \
	btnpanel.h     \
	callbacks.c     \
	callbacks.h     \
	calltip.c     \
	calltip.h     \
	dbm_gdb.c     \
	dconfig.c     \
	dconfig.h     \
	debug.c     \
	debug.h     \
	debug_module.c     \
	debug_module.h     \
	dpaned.c     \
	dpaned.h     \
	envtree.c     \
	envtree.h     \
	gui.h     \
	gui.c     \
	keys.c     \
	keys.h     \
	atree.c     \
	atree.h     \
	markers.c     \
	markers.h     \
	pixbuf.c     \
	pixbuf.h     \
	plugin.c     \
	stree.c     \
	stree.h     \
	tabs.c     \
	tabs.h     \
	tpage.c     \
	tpage.h     \
	utils.c     \
	utils.h     \
	vtree.c     \
	vtree.h     \
	watch_model.c     \
	watch_model.h     \
	wtree.c     \
	wtree.h

debugger_la_LIBADD = $(COMMONLIBS) $(VTE_LIBS) -lutil
debugger_la_CFLAGS = $(AM_CFLAGS) $(VTE_CFLAGS) -DDBGPLUG_DATA_DIR=\"$(plugindatadir)\" -DPLUGIN_NAME=\"$(plugin)\"

include $(top_srcdir)/build/cppcheck.mk
