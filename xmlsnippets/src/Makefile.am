include $(top_srcdir)/build/vars.build.mk
plugin = xmlsnippets

geanyplugins_LTLIBRARIES = xmlsnippets.la

check_PROGRAMS = xmlsnippets-check

common_sources = xmlsnippets.c xmlsnippets.h

xmlsnippets_la_SOURCES = \
	plugin.c plugin.h \
	$(common_sources)

xmlsnippets_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"XMLSnippets\"
xmlsnippets_la_LIBADD = $(COMMONLIBS)

xmlsnippets_check_SOURCES = \
	tests.c \
	test-stubs.c test-stubs.h \
	$(common_sources)

xmlsnippets_check_CPPFLAGS = -DTEST
xmlsnippets_check_LDADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
