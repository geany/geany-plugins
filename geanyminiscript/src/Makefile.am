include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYMINISCRIPT
geanyplugins_LTLIBRARIES = geanyminiscript.la
else
EXTRA_LTLIBRARIES = geanyminiscript.la
endif

geanyminiscript_la_SOURCES = gms.c gms_gui.c
geanyminiscript_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
