include $(top_srcdir)/build/vars.build.mk
plugin = lipsum

geanyplugins_LTLIBRARIES = lipsum.la

lipsum_la_SOURCES = lipsum.c
lipsum_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Lipsum\"
lipsum_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
