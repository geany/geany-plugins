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
	dialogs.h \
	dialogs.c \
	menu.h \
	menu.c \
	popup_menu.h \
	popup_menu.c \
	sidebar.h \
	sidebar.c \
	utils.h \
	utils.c

workbench_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Workbench\"
workbench_la_CFLAGS = $(AM_CFLAGS)
workbench_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk

