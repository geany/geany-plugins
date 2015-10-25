include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = addons.la

addons_la_SOURCES = \
	addons.h \
	ao_blanklines.h \
	ao_doclist.h \
	ao_openuri.h \
	ao_systray.h \
	ao_bookmarklist.h \
	ao_markword.h \
	ao_tasks.h \
	ao_xmltagging.h \
	ao_wrapwords.h \
	ao_copyfilepath.h \
	addons.c \
	ao_blanklines.c \
	ao_doclist.c \
	ao_openuri.c \
	ao_systray.c \
	ao_bookmarklist.c \
	ao_markword.c \
	ao_tasks.c \
	ao_xmltagging.c \
	ao_wrapwords.c \
	ao_copyfilepath.c

addons_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
