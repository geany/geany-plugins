include $(top_srcdir)/build/vars.docs.mk

plugin = geanylua
exampleinfodir = $(examplesdir)/info
EXAMPLES = \
	about.lua \
	app-information.lua \
	file-information.lua \
	list-open-files.lua \
	Makefile.am \
	show-filename.lua

if ENABLE_GEANYLUA
exampleinfo_DATA = $(EXAMPLES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(EXAMPLES)
