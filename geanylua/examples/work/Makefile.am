# avoid creating empty directories if disabled
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.docs.mk
plugin = geanylua
workdir = $(examplesdir)/work
endif

EXAMPLES = \
	01.edit-test-script.lua \
	02.run-test-script.lua \
	03.install-test-script.lua

if ENABLE_GEANYLUA
work_DATA = $(EXAMPLES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(EXAMPLES)
