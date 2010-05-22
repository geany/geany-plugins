if ENABLE_GEANYGENDOC
include $(top_srcdir)/build/vars.docs.mk
plugin = geanygendoc
AIXFILES=
endif ENABLE_GEANYGENDOC

EXTRA_DIST = manual.rst \
             manual.css \
             html4css1.css

if ENABLE_GEANYGENDOC
if BUILD_RST
plugindoc_DATA = manual.html

EXTRA_DIST += manual.html

manual.html: manual.rst manual.css
	$(AM_V_GEN) $(RST2HTML) -d --strict --stylesheet-path manual.css $< $@
endif BUILD_RST
endif ENABLE_GEANYGENDOC
