include $(top_srcdir)/build/vars.build.mk
plugin = addons

geanyplugins_LTLIBRARIES = addons.la

addons_la_SOURCES = \
	addons.c \
	addons.h \
	ao_blanklines.c \
	ao_blanklines.h \
	ao_bookmarklist.c \
	ao_bookmarklist.h \
	ao_colortip.c \
	ao_colortip.h \
	ao_copyfilepath.c \
	ao_copyfilepath.h \
	ao_doclist.c \
	ao_doclist.h \
	ao_markword.c \
	ao_markword.h \
	ao_openuri.c \
	ao_openuri.h \
	ao_systray.c \
	ao_systray.h \
	ao_tasks.c \
	ao_tasks.h \
	ao_wrapwords.c \
	ao_wrapwords.h \
	ao_xmltagging.c \
	ao_xmltagging.h

addons_la_CPPFLAGS = $(AM_CPPFLAGS) \
					 -DG_LOG_DOMAIN=\"Addons\"

addons_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
