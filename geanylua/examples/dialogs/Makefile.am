# avoid creating empty directories if disabled
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.docs.mk
plugin = geanylua
dialogsdir = $(examplesdir)/dialogs
endif

EXAMPLES = \
	basic-dialogs.lua \
	complex-dialog.lua

if ENABLE_GEANYLUA
dialogs_DATA = $(EXAMPLES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(EXAMPLES)
