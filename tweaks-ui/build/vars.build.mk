if MINGW
GP_PREFIX				= .
GP_DATADIR				= $(GP_PREFIX)/share
GP_LOCALEDIR			= $(GP_DATADIR)/locale
GP_DOCDIR				= $(GP_DATADIR)/doc/$(PACKAGE)
GP_PKGDATADIR			= $(GP_DATADIR)/$(PACKAGE)
GP_LIBDIR				= $(GP_PREFIX)/lib
GP_PKGLIBDIR			= $(GP_LIBDIR)/$(PACKAGE)
else
GP_PREFIX				= $(prefix)
GP_DATADIR				= $(datadir)
GP_LOCALEDIR			= $(LOCALEDIR)
GP_DOCDIR				= $(docdir)
GP_PKGDATADIR			= $(pkgdatadir)
GP_LIBDIR				= $(libdir)
GP_PKGLIBDIR			= $(pkglibdir)
endif

LOCAL_AM_CFLAGS = \
	-DLOCALEDIR=\""$(GP_LOCALEDIR)"\" \
	-DPREFIX=\""$(GP_PREFIX)"\" \
	-DDOCDIR=\""$(GP_DOCDIR)"\" \
	-DGEANYPLUGINS_DATADIR=\""$(GP_DATADIR)"\" \
	-DPKGDATADIR=\""$(GP_PKGDATADIR)"\" \
	-DLIBDIR=\""$(GP_LIBDIR)"\" \
	-DPKGLIBDIR=\""$(GP_PKGLIBDIR)"\" \
	-DPLUGINDATADIR=\""$(GP_PKGDATADIR)/$(plugin)"\" \
	-DPLUGINDOCDIR=\""$(GP_DOCDIR)/$(plugin)"\" \
	-DPLUGINLIBDIR=\""$(GP_PKGLIBDIR)/$(plugin)"\" \
	-DPLUGIN="\"$(plugin)\""

AM_CFLAGS = \
	${LOCAL_AM_CFLAGS} \
	$(GEANY_CFLAGS) \
	$(GP_CFLAGS)


AM_LDFLAGS = -module -avoid-version -no-undefined $(GP_LDFLAGS)

COMMONLIBS = \
	$(GEANY_LIBS) \
	$(INTLLIBS)
