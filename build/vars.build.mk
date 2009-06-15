AM_CFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DDOCDIR=\""$(docdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	$(GEANY_CFLAGS)

AM_LDFLAGS = -module -avoid-version

COMMONLIBS = \
	$(GEANY_LIBS) \
	$(INTLLIBS)

# delete all .la files
install-data-hook:
	cd $(DESTDIR)$(geanypluginsdir); \
	rm -f $(geanyplugins_LTLIBRARIES)
