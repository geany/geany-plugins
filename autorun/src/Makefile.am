include $(top_srcdir)/build/vars.build.mk
plugin = autorun

geanyplugins_LTLIBRARIES = autorun.la

autorun_la_SOURCES = \
	plugin_main.c \
	autorun.c \
	utils.c \
	spawn.c \
	menu.c \
	project.c

autorun_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Autorun\"
autorun_la_CFLAGS = $(AM_CFLAGS) $(WORKBENCH_CFLAGS) \
	-I$(top_srcdir)/utils/src
autorun_la_LIBADD = $(COMMONLIBS) \
	$(AUTORUN_LIBS) \
	$(top_builddir)/utils/src/libgeanypluginutils.la
include $(top_srcdir)/build/cppcheck.mk
