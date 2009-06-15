plugindocdir = $(docdir)/$(plugin)
plugindoc_DATA = \
	README \
	ChangeLog \
	NEWS \
	COPYING \
	AUTHORS

# TODO: make sure these files exist!
README AUTHORS:
	touch $@
