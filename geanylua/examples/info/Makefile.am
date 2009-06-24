# avoid creating empty directories if disabled
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.docs.mk
plugin = geanylua
exampleinfodir = $(examplesdir)/info
endif

EXAMPLES = \
	about.lua \
	app-information.lua \
	file-information.lua \
	list-open-files.lua \
	show-filename.lua

if ENABLE_GEANYLUA
exampleinfo_DATA = $(EXAMPLES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(EXAMPLES)
