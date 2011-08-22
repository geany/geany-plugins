include $(top_srcdir)/build/vars.docs.mk

plugin = debugger
FILES = \
	continue.png \
	restart.gif \
	run.gif \
	run_to_cursor.gif \
	step_in.png \
	step_out.gif \
	step_over.gif \
	stop.gif \
	tab.gif \
	tabs.gif

EXTRA_DIST = $(FILES)

if ENABLE_DEBUGGER
plugindata_DATA = $(FILES)
else
EXTRA_DATA = $(FILES)
endif
