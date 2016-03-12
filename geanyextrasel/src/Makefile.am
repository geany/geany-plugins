include $(top_srcdir)/build/vars.build.mk
plugin = geanyextrasel

geanyplugins_LTLIBRARIES = geanyextrasel.la

geanyextrasel_la_SOURCES = extrasel.c
geanyextrasel_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"ExtraSelection\"
geanyextrasel_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
