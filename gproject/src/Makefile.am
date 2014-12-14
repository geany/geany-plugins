include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = gproject.la

gproject_la_SOURCES = \
	gproject-main.c \
	gproject-project.h \
	gproject-project.c \
	gproject-sidebar.h \
	gproject-sidebar.c \
	gproject-utils.h \
	gproject-utils.c \
	gproject-menu.h \
	gproject-menu.c

gproject_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DPLUGIN=\"$(plugin)\" \
	-DG_LOG_DOMAIN=\"GProject\"
gproject_la_CFLAGS = $(AM_CFLAGS)
gproject_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk

