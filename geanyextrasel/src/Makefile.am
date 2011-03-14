include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYEXTRASEL
geanyplugins_LTLIBRARIES = geanyextrasel.la
else
EXTRA_LTLIBRARIES = geanyextrasel.la
endif

geanyextrasel_la_SOURCES = extrasel.c
geanyextrasel_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
