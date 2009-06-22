# avoid creating empty directories if disabled
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.docs.mk
plugin = geanylua
scriptingdir = $(examplesdir)/scripting
endif

EXAMPLES = \
	help.lua \
	open-script.lua \
	rebuild-menu.lua \
	show-examples.lua

if ENABLE_GEANYLUA
scripting_DATA = $(EXAMPLES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(EXAMPLES)
