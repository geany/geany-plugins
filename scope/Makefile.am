include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = scope

geanyplugins_LTLIBRARIES = scope.la

scope_la_SOURCES = \
	src/break.c \
	src/break.h \
	src/common.h \
	src/conterm.c \
	src/conterm.h \
	src/debug.c \
	src/debug.h \
	src/gtk216.c \
	src/gtk216.h \
	src/inspect.c \
	src/inspect.h \
	src/local.c \
	src/local.h \
	src/memory.c \
	src/memory.h \
	src/menu.c \
	src/menu.h \
	src/parse.c \
	src/parse.h \
	src/plugme.c \
	src/plugme.h \
	src/prefs.c \
	src/prefs.h \
	src/program.c \
	src/program.h \
	src/register.c \
	src/register.h \
	src/scope.c \
	src/scope.h \
	src/stack.c \
	src/stack.h \
	src/thread.c \
	src/thread.h \
	src/tooltip.c \
	src/tooltip.h \
	src/utils.c \
	src/utils.h \
	src/views.c \
	src/views.h \
	src/watch.c \
	src/watch.h \
	src/store.h \
	src/store/scptreedata.h \
	src/store/scptreedata.c \
	src/store/scptreestore.h \
	src/store/scptreestore.c

scope_la_LIBADD = $(COMMONLIBS) $(VTE_LIBS) $(PTY_LIBS)

scope_la_CFLAGS = $(AM_CFLAGS) $(VTE_CFLAGS) \
	-DPLUGINDATADIR=\"$(plugindatadir)\" \
	-DPLUGINHTMLDOCDIR=\"$(plugindocdir)/html\" \
	-Wno-shadow

# static analysis
include $(top_srcdir)/build/cppcheck.mk

# data
dist_plugindata_DATA = \
	data/BreakPoint.png \
	data/BreakPoint22.png \
	data/Continue.png \
	data/Continue22.png \
	data/Continue24.png \
	data/Kill.png \
	data/Kill22.png \
	data/Kill24.png \
	data/Pause.png \
	data/Pause22.png \
	data/Pause24.png \
	data/RunToCursor.png \
	data/RunToCursor22.png \
	data/RunToCursor24.png \
	data/RunToMethod.png \
	data/RunToMethod22.png \
	data/RunToMethod24.png \
	data/StepInto.png \
	data/StepInto22.png \
	data/StepInto24.png \
	data/StepOut.png \
	data/StepOut22.png \
	data/StepOut24.png \
	data/StepOver.png \
	data/StepOver22.png \
	data/StepOver24.png \
	data/scope.glade

# documentation
pluginhtmldocdir = $(plugindocdir)/html
dist_pluginhtmldoc_DATA = \
	docs/scope.html \
	docs/codes.html
