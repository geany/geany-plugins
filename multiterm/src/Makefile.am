include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk

plugin = multiterm

geanyplugins_LTLIBRARIES = multiterm.la

multiterm_la_VALAFLAGS = \
	--vapidir $(top_srcdir)/multiterm/src/vapi

if GP_GTK3
multiterm_la_VALAFLAGS += \
	--pkg geany-gtk3 \
	--pkg gtk+-3.0 \
	--pkg vte-2.90 \
	-D MULTITERM_GTK3
else
multiterm_la_VALAFLAGS += \
	--pkg geany \
	--pkg gtk+-2.0 \
	--pkg vte
endif

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
