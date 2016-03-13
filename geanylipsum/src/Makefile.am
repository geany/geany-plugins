include $(top_srcdir)/build/vars.build.mk
plugin = geanylipsum

geanyplugins_LTLIBRARIES = geanylipsum.la

geanylipsum_la_SOURCES = geanylipsum.c
geanylipsum_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyLipsum\"
geanylipsum_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
