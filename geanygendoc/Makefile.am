if ENABLE_GEANYGENDOC
include $(top_srcdir)/build/vars.auxfiles.mk
else
include $(top_srcdir)/build/vars.docs.mk
EXTRA_DIST = $(AUXFILES)
endif

SUBDIRS = src docs
# don't install data because it doesn't work for now
#SUBDIRS += data
plugin = geanygendoc
