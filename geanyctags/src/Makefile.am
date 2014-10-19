include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = geanyctags.la

geanyctags_la_SOURCES = \
	geanyctags.c \
	readtags.h \
	readtags.c

geanyctags_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
