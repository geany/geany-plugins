if MINGW
LOCAL_AM_CFLAGS = \
	-DLOCALEDIR=\""share/locale"\" \
	-DPREFIX=\"\" \
	-DDOCDIR=\""share/doc/$(PACKAGE)"\" \
	-DGEANYPLUGINS_DATADIR=\""share"\" \
	-DPKGDATADIR=\""share/$(PACKAGE)"\" \
	-DLIBDIR=\""lib"\" \
	-DPKGLIBDIR=\""lib/$(PACKAGE)"\"
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
