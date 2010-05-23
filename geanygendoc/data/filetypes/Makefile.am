if ENABLE_GEANYGENDOC
plugin = geanygendoc
# FIXME: should probably done by an included .mk such as vars.data.mk
plugindatadir = $(pkgdatadir)/$(plugin)
endif

FILETYPES = c.conf \
            vala.conf

if ENABLE_GEANYGENDOC
filetypesdir = $(plugindatadir)/filetypes
filetypes_DATA = $(FILETYPES)
endif

EXTRA_DIST = $(FILETYPES)
