include $(top_srcdir)/build/vars.build.mk
plugin = jacococoverage

geanyplugins_LTLIBRARIES = jacococoverage.la

jacococoverage_la_SOURCES = jacococoverage.c
jacococoverage_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"JaCoCoCoverage\"
jacococoverage_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
