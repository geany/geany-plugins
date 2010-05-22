if ENABLE_GEANYGENDOC
plugin = geanygendoc
endif

FILETYPES = c.conf \
            vala.conf

if ENABLE_GEANYGENDOC
filetypesdir = $(plugindatadir)/filetypes
filetypes_DATA = $(FILETYPES)
endif

EXTRA_DIST = $(FILETYPES)
