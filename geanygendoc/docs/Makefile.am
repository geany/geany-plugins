include $(top_srcdir)/build/vars.docs.mk
plugin = geanygendoc
pluginhtmldocdir = $(plugindocdir)/html

# CSS used in the manual, in the order it should be used
MANUAL_CSS = \
	html4css1.css \
	manual.css

EXTRA_DIST = $(MANUAL_CSS)

dist_plugindoc_DATA = \
	manual.rst

dist_pluginhtmldoc_DATA = \
	manual.html

if BUILD_RST
manual-combined.css: $(MANUAL_CSS)
	$(AM_V_GEN) cat $^ > $@
manual.html: manual.rst manual-combined.css
	$(AM_V_GEN) $(RST2HTML) -d --strict \
		--stylesheet-path manual-combined.css \
		$(srcdir)/manual.rst $@

CLEANFILES = \
	manual-combined.css \
	manual.html

endif BUILD_RST
