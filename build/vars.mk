AM_CFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	$(GEANY_CFLAGS)

AM_LDFLAGS = -module -avoid-version

COMMONLIBS = \
	$(GEANY_LIBS)
	
