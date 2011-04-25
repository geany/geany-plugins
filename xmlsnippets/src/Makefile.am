include $(top_srcdir)/build/vars.build.mk

if ENABLE_XMLSNIPPETS
geanyplugins_LTLIBRARIES = xmlsnippets.la
else
EXTRA_LTLIBRARIES = xmlsnippets.la
endif

xmlsnippets_la_SOURCES = \
	plugin.c plugin.h \
	tests.c \
	test-stubs.c test-stubs.h \
	xmlsnippets.c xmlsnippets.h

xmlsnippets_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
