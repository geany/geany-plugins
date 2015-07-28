include $(top_srcdir)/build/vars.init.mk

dist_plugindoc_DATA += \
	README \
	ChangeLog \
	NEWS \
	COPYING \
	AUTHORS \
	$(AUXFILES)

EXTRA_DIST += \
	wscript_build \
	wscript_configure
