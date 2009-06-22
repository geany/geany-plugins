# if spellcheck is disabled, we should distribute its files, but not install
if ENABLE_SPELLCHECK
include $(top_srcdir)/build/vars.auxfiles.mk
else
include $(top_srcdir)/build/vars.docs.mk
EXTRA_DIST = $(AUXFILES)
endif

SUBDIRS = src
plugin = spellcheck
