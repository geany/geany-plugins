include $(top_srcdir)/build/vars.docs.mk
plugin = geanygendoc
pluginhtmldocdir = $(plugindocdir)/html

EXTRA_DIST = \
	manual.css \
	html4css1.css

dist_plugindoc_DATA = \
	manual.rst

dist_pluginhtmldoc_DATA = \
	manual.html

if BUILD_RST
manual.html: manual.rst manual.css html4css1.css
	$(AM_V_GEN) $(RST2HTML) -d --strict --stylesheet-path html4css1.css,manual.css $< $@
endif BUILD_RST
