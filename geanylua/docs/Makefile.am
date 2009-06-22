# don't include if geanylua's disabled, otherwise a whole bunch of empty
# directories will be created
if ENABLE_GEANYLUA
include $(top_srcdir)/build/vars.docs.mk
endif

plugin = geanylua

DOCFILES = \
	geanylua-index.html \
	geanylua-input.html \
	geanylua-intro.html \
	geanylua-keyfile.html \
	geanylua-keys.html \
	geanylua-ref.html \
	luarefv51.html

if ENABLE_GEANYLUA
htmldoc_DATA = $(DOCFILES)
endif

# make sure that these files are distributed as well
EXTRA_DIST = $(DOCFILES)
