include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk

plugin = xmlsnippets

geanyplugins_LTLIBRARIES = xmlsnippets.la

xmlsnippets_la_SOURCES = \
	src/plugin.c src/plugin.h \
	src/tests.c \
	src/test-stubs.c src/test-stubs.h \
	src/xmlsnippets.c src/xmlsnippets.h

xmlsnippets_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
