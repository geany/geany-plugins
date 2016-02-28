if MINGW
LOCAL_AM_CFLAGS = \
	-DLOCALEDIR=\""share/locale"\" \
	-DPREFIX=\"\" \
	-DDOCDIR=\""share/doc/geany-plugins"\" \
	-DGEANYPLUGINS_DATADIR=\""share"\" \
	-DPKGDATADIR=\""share/geany-plugins"\" \
	-DLIBDIR=\""lib"\" \
	-DPKGLIBDIR=\""lib/geany-plugins"\"
else
LOCAL_AM_CFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DDOCDIR=\""$(docdir)"\" \
	-DGEANYPLUGINS_DATADIR=\""$(datadir)"\" \
	-DPKGDATADIR=\""$(pkgdatadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DPKGLIBDIR=\""$(pkglibdir)"\"
endif

AM_CFLAGS = \
	${LOCAL_AM_CFLAGS} \
	$(GEANY_CFLAGS) \
	$(GP_CFLAGS)


AM_LDFLAGS = -module -avoid-version -no-undefined $(GP_LDFLAGS)

COMMONLIBS = \
	$(GEANY_LIBS) \
	$(INTLLIBS)
