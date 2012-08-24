include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = xmlsnippets.la

xmlsnippets_la_SOURCES = \
	plugin.c plugin.h \
	tests.c \
	test-stubs.c test-stubs.h \
	xmlsnippets.c xmlsnippets.h

xmlsnippets_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
