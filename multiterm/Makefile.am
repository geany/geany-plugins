include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk
plugin = multiterm

AUXFILES = TODO

geanyplugins_LTLIBRARIES = multiterm.la

multiterm_la_VALAFLAGS = \
	--vapidir $(abs_top_srcdir)/multiterm/src/vapi \
	--pkg gtk+-2.0 \
	--pkg geany \
	--pkg vte \
	--header src/multiterm.h \
	--use-header

multiterm_la_SOURCES = \
	src/config.vala \
	src/context-menu.vala \
	src/defconf.vala \
	src/notebook.vala \
	src/plugin.vala \
	src/shell-config.vala \
	src/tab-label.vala \
	src/terminal.vala

multiterm_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MULTITERM_CFLAGS)

multiterm_la_LIBADD = \
	$(MULTITERM_LIBS)
