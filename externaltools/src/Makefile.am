include $(top_srcdir)/build/vars.build.mk
plugin = externaltools

geanyplugins_LTLIBRARIES = externaltools.la

externaltools_la_SOURCES = externaltools.c

externaltools_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"ExternalTools\"
externaltools_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
