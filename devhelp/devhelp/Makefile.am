noinst_LTLIBRARIES = libdevhelp-2.la

dh_headers = \
	dh-assistant.h \
	dh-assistant-view.h \
	dh-base.h \
	dh-book-manager.h \
	dh-book.h \
	dh-book-tree.h \
	dh-error.h \
	dh-keyword-model.h \
	dh-link.h \
	dh-search.h \
	dh-window.h

dh-enum-types.h: dh-enum-types.h.template $(dh_headers) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template dh-enum-types.h.template $(dh_headers)) > $@

dh-enum-types.c: dh-enum-types.c.template $(dh_headers) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template dh-enum-types.c.template $(dh_headers)) > $@

BUILT_SOURCES = \
	dh-marshal.h \
	dh-marshal.c \
	dh-enum-types.h \
	dh-enum-types.c

EXTRA_DIST = \
	dh-marshal.list \
	dh-enum-types.c.template \
	dh-enum-types.h.template

libdevhelp_2_la_SOURCES = \
	dh-assistant.c \
	dh-assistant-view.c \
	dh-base.c \
	dh-book.c \
	dh-book-manager.c \
	dh-book-tree.c \
	dh-enum-types.c \
	dh-enum-types.h \
	dh-error.c \
	dh-keyword-model.c \
	dh-link.c \
	dh-marshal.c \
	dh-marshal.h \
	dh-parser.c \
	dh-parser.h \
	dh-preferences.c \
	dh-preferences.h \
	dh-search.c \
	dh-util.c \
	dh-util.h \
	dh-window.c \
	eggfindbar.c \
	eggfindbar.h \
	ige-conf.c \
	ige-conf-gconf.c \
	ige-conf.h \
	ige-conf-private.h \
	$(dh_headers)

libdevhelp_2_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-DLOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DG_LOG_DOMAIN=\"Devhelp\" \
	$(DEVHELP_CPPFLAGS)

libdevhelp_2_la_CFLAGS = \
	$(DEVHELP_CFLAGS)

libdevhelp_2_la_LIBADD = \
	$(DEVHELP_LIBS)

libdevhelp_2_la_LDFLAGS = \
	-no-undefined

dh-marshal.h: dh-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=_dh_marshal --header --output=$@ dh-marshal.list

dh-marshal.c: dh-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=_dh_marshal --body --output=$@ dh-marshal.list
