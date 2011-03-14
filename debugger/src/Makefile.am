include $(top_srcdir)/build/vars.build.mk

if ENABLE_DEBUGGER
geanyplugins_LTLIBRARIES = debugger.la
else
EXTRA_LTLIBRARIES = debugger.la
endif

debugger_la_SOURCES = \
	bptree.c     \
	bptree.h     \
	breakpoint.c     \
	breakpoint.h     \
	breakpoints.c     \
	breakpoints.h     \
	callbacks.c     \
	callbacks.h     \
	dbm_gdb.c     \
	debug.c     \
	debug.h     \
	debug_module.c     \
	debug_module.h     \
	keys.c     \
	keys.h     \
	ltree.c     \
	ltree.h     \
	markers.c     \
	markers.h     \
	plugin.c     \
	stree.c     \
	stree.h     \
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

debugger_la_LIBADD = $(COMMONLIBS) -lutil
debugger_la_CFLAGS = $(AM_CFLAGS) $(VTE_CFLAGS)

include $(top_srcdir)/build/cppcheck.mk
