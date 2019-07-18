include $(top_srcdir)/build/vars.build.mk
plugin = workbench

geanyplugins_LTLIBRARIES = workbench.la

workbench_la_SOURCES = \
	plugin_main.c \
	wb_globals.h \
	wb_globals.c \
	workbench.h \
	workbench.c \
	wb_project.h \
	wb_project.c \
	wb_monitor.h \
	wb_monitor.c \
	dialogs.h \
	dialogs.c \
	search_projects.h \
	search_projects.c \
	menu.h \
	menu.c \
	popup_menu.h \
	popup_menu.c \
	sidebar.h \
	sidebar.c \
	utils.h \
	utils.c \
	idle_queue.h \
	idle_queue.c \
	tm_control.h \
	tm_control.c

workbench_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Workbench\"
workbench_la_CFLAGS = $(AM_CFLAGS) $(WORKBENCH_CFLAGS) \
	-I$(top_srcdir)/utils/src
workbench_la_LIBADD = $(COMMONLIBS) \
	$(WORKBENCH_LIBS) \
	$(top_builddir)/utils/src/libgeanypluginutils.la
include $(top_srcdir)/build/cppcheck.mk

