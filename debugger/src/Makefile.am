include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

geanyplugins_LTLIBRARIES = debugger.la

plugin = debugger

debugger_la_SOURCES = \
	xpm/breakpoint_condition.xpm     \
	xpm/breakpoint_disabled.xpm     \
	xpm/breakpoint.xpm     \
	xpm/frame_current.xpm     \
	xpm/frame.xpm     \
	xpm/argument.xpm     \
	xpm/local.xpm     \
	xpm/watch.xpm     \
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
	gdb_mi.h  \
	gdb_mi.c  \
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
	wtree.h		\
	cell_renderers/cellrendererbreakicon.c \
	cell_renderers/cellrendererbreakicon.h \
	cell_renderers/cellrendererframeicon.c \
	cell_renderers/cellrendererframeicon.h \
	cell_renderers/cellrenderertoggle.c \
	cell_renderers/cellrenderertoggle.h

debugger_la_LIBADD = $(COMMONLIBS) $(VTE_LIBS) -lutil
debugger_la_CFLAGS = $(AM_CFLAGS) $(VTE_CFLAGS) -DDBGPLUG_DATA_DIR=\"$(plugindatadir)\" -DPLUGIN_NAME=\"$(plugin)\"

check_PROGRAMS = gdb_mi_test
dist_check_SCRIPTS = tests/gdb_mi_test.sh
dist_check_DATA = tests/gdb_mi_test.input tests/gdb_mi_test.expected
TESTS = $(dist_check_SCRIPTS)

gdb_mi_test_SOURCES = gdb_mi.c gdb_mi.h
gdb_mi_test_CFLAGS = $(AM_CFLAGS) -DTEST
gdb_mi_test_LDFLAGS =
gdb_mi_test_LDADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
