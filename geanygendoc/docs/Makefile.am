if ENABLE_GEANYGENDOC
include $(top_srcdir)/build/vars.docs.mk
plugin = geanygendoc
pluginhtmldocdir = $(plugindocdir)/html
endif ENABLE_GEANYGENDOC

EXTRA_DIST = manual.rst \
             manual.css \
             html4css1.css \
             manual.html

if ENABLE_GEANYGENDOC
plugindoc_DATA = manual.rst
pluginhtmldoc_DATA = manual.html

if BUILD_RST
manual.html: manual.rst manual.css
	$(AM_V_GEN) $(RST2HTML) -d --strict --stylesheet-path manual.css $< $@
endif BUILD_RST
endif ENABLE_GEANYGENDOC
