include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = tableconvert

geanyplugins_LTLIBRARIES = tableconvert.la

tableconvert_la_SOURCES = \
	src/tableconvert.c \
	src/tableconvert.h \
	src/tableconvert_ui.c \
	src/tableconvert_ui.h
tableconvert_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
