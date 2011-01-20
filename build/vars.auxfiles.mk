include $(top_srcdir)/build/vars.docs.mk

plugindoc_DATA = $(AUXFILES)
EXTRA_DIST = \
	wscript_build \
	wscript_configure
