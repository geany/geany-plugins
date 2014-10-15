include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

geanyplugins_LTLIBRARIES = numconvert.la

numconvert_la_VALAFLAGS = \
	--vapidir=$(geanyvapidir) \
	--pkg gtk+-2.0 \
	--pkg geany \
	--header numconvert.h \
	--use-header

numconvert_la_SOURCES = \
	plugin.vala \
	context-menu-all.vala \
	context-menu.vala \
	number-parser.vala  \
	convert-menu-item.vala \
	converters.vala

CLEANFILES = $(numconvert_la_SOURCES:.vala=.c) \
	numconvert_la_vala.stamp

numconvert_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NUMCONVERT_CFLAGS)

numconvert_la_LIBADD = \
	$(NUMCONVERT_LIBS)
