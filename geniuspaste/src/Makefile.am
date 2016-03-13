include $(top_srcdir)/build/vars.build.mk
include $(top_srcdir)/build/vars.docs.mk
plugin = geniuspaste

geanyplugins_LTLIBRARIES = geniuspaste.la

geniuspaste_la_SOURCES = geniuspaste.c
geniuspaste_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DGEANY_VERSION=\"$(GEANY_VERSION)\" \
	-DG_LOG_DOMAIN=\"GeniusPaste\"
geniuspaste_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GENIUSPASTE_CFLAGS)
geniuspaste_la_LIBADD = \
	$(COMMONLIBS) \
	$(GENIUSPASTE_LIBS)

include $(top_srcdir)/build/cppcheck.mk
