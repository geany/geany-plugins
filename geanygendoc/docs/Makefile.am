if ENABLE_GEANYGENDOC
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
manual.html: manual.rst manual.css
	$(AM_V_GEN) $(RST2HTML) -d --strict --stylesheet-path manual.css $< $@
endif BUILD_RST

endif ENABLE_GEANYGENDOC
