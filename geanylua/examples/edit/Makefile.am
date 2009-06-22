# avoid creating empty directories if disabled
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.docs.mk
plugin = geanylua
editdir = $(examplesdir)/edit
endif

EXAMPLES = \
	calculator.lua \
	lua-replace.lua \
	proper-case.lua \
	reverse.lua \
	right-trim.lua \
	select-block.lua

if ENABLE_GEANYLUA
edit_DATA = $(EXAMPLES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(EXAMPLES)
