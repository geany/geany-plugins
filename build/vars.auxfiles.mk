include $(top_srcdir)/build/vars.docs.mk

plugindoc_DATA = $(AUXFILES)

# TODO: make sure these files exist!
README AUTHORS NEWS:
	touch $@
