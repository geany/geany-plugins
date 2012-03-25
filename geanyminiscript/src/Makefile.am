include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYMINISCRIPT
geanyplugins_LTLIBRARIES = geanyminiscript.la
else
EXTRA_LTLIBRARIES = geanyminiscript.la
endif

geanyminiscript_la_SOURCES = gms.c gms.h gms_gui.c gms_gui.h gms_debug.h
geanyminiscript_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
