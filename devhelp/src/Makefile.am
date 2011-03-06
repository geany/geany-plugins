ACLOCAL_AMFLAGS = -I m4

geanypluginsdir 					= $(libdir)/geany
geanyplugins_LTLIBRARIES	= devhelp.la

devhelp_la_LDFLAGS 				= -module -avoid-version -shared
devhelp_la_CPPFLAGS 			= @GTK_CFLAGS@			\
														@GEANY_CFLAGS@		\
														@DEVHELP_CFLAGS@	\
														-DDHPLUG_DATA_DIR=\"$(pkgdatadir)\"
devhelp_la_LIBADD 				= @GTK_LIBS@ @GEANY_LIBS@ @DEVHELP_LIBS@
devhelp_la_SOURCES				= plugin.c dh-plugin.c main-notebook.c
