AM_CFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DDOCDIR=\""$(docdir)"\" \
	-DGEANYPLUGINS_DATADIR=\""$(datadir)"\" \
	-DPKGDATADIR=\""$(pkgdatadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DPKGLIBDIR=\""$(pkglibdir)"\" \
	$(GEANY_CFLAGS)

AM_LDFLAGS = -module -avoid-version

COMMONLIBS = \
	$(GEANY_LIBS) \
	$(INTLLIBS)
