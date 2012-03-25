include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

plugin = multiterm

if ENABLE_MULTITERM
geanyplugins_LTLIBRARIES = multiterm.la
else
EXTRA_LTLIBRARIES = multiterm.la
endif

VALAFLAGS = \
	--vapidir $(top_srcdir)/multiterm/src/vapi \
	--pkg gtk+-2.0 \
	--pkg geany \
	--pkg vte \
	--header=multiterm.h \
	--use-header

multiterm_la_SOURCES = \
	config.vala \
	context-menu.vala \
	defconf.vala \
	notebook.vala \
	plugin.vala \
	shell-config.vala \
	tab-label.vala \
	terminal.vala

multiterm_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MULTITERM_CFLAGS)

multiterm_la_LIBADD = \
	$(MULTITERM_LIBS)

EXTRA_DIST = multiterm.h
