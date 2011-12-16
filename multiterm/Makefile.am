if ENABLE_MULTITERM
include $(top_srcdir)/build/vars.auxfiles.mk
else
include $(top_srcdir)/build/vars.docs.mk
EXTRA_DIST = $(AUXFILES)
endif

AUXFILES += TODO

SUBDIRS = src data
plugin = multiterm
