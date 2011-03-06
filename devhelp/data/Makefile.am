include $(top_srcdir)/build/vars.docs.mk

FILES = \
	devhelp-plugin.svg \
	devhelp-plugin-48.png \
	geany-devhelp-plugin.png \
	home.html \
	devhelp.conf

EXTRA_DIST = $(FILES)

if ENABLE_DEVHELP
plugindata_DATA = $(FILES)
else
EXTRA_DATA = $(FILES)
endif
