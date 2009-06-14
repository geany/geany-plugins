geanyplugins_LTLIBRARIES = \
	addons.la

addons_la_SOURCES = \
	addons.h \
	ao_doclist.h \
	ao_openuri.h \
	ao_systray.h \
	tasks.h \
	addons.c \
	ao_doclist.c \
	ao_openuri.c \
	ao_systray.c \
	tasks.c

addons_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GEANY_CFLAGS) \
	-DLOCALEDIR=\"$(LOCALEDIR)\"

addons_la_LIBADD = \
	$(GEANY_LIBS)

addons_la_LDFLAGS = -module -avoid-version
