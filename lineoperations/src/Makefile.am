include $(top_srcdir)/build/vars.build.mk
plugin = lineoperations

geanyplugins_LTLIBRARIES = lineoperations.la

lineoperations_la_SOURCES = \
	linefunctions.h \
	linefunctions.c \
	lineoperations.c

lineoperations_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"LineOperations\"
lineoperations_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
