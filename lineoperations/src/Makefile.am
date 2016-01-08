include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = lineoperations.la

lineoperations_la_SOURCES = \
	linefunctions.h \
	linefunctions.c \
	lineoperations.c

lineoperations_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
