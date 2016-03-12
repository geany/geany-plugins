include $(top_srcdir)/build/vars.build.mk
plugin = geanyinsertnum

geanyplugins_LTLIBRARIES = geanyinsertnum.la

geanyinsertnum_la_SOURCES = insertnum.c
geanyinsertnum_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"InsertNum\"
geanyinsertnum_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
