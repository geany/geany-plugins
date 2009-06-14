AM_CFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DDOCDIR=\""$(docdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	$(GEANY_CFLAGS)

AM_LDFLAGS = -module -avoid-version

COMMONLIBS = \
	$(GEANY_LIBS)
	
