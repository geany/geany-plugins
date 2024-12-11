include $(top_srcdir)/build/vars.build.mk
plugin = projectorganizer

geanyplugins_LTLIBRARIES = projectorganizer.la

projectorganizer_la_SOURCES = \
	prjorg-main.c \
	prjorg-project.h \
	prjorg-project.c \
	prjorg-sidebar.h \
	prjorg-sidebar.c \
	prjorg-utils.h \
	prjorg-utils.c \
	prjorg-menu.h \
	prjorg-menu.c \
	prjorg-goto-panel.h \
	prjorg-goto-panel.c \
	prjorg-goto-anywhere.h \
	prjorg-goto-anywhere.c \
	prjorg-wraplabel.h \
	prjorg-wraplabel.c

projectorganizer_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"ProjectOrganizer\"
projectorganizer_la_CFLAGS = $(AM_CFLAGS)
projectorganizer_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk

