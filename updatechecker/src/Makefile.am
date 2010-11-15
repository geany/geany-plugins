include $(top_srcdir)/build/vars.build.mk
plugin = updatechecker

if ENABLE_UPDATECHECKER
geanyplugins_LTLIBRARIES = updatechecker.la
else
EXTRA_LTLIBRARIES = updatechecker.la
endif

updatechecker_la_SOURCES = updatechecker.c
updatechecker_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DGEANY_VERSION=\"$(GEANY_VERSION)\" \
	-DG_LOG_DOMAIN=\"Updatechecker\"
updatechecker_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(UPDATECHECKER_CFLAGS)
updatechecker_la_LIBADD = \
	$(COMMONLIBS) \
	$(UPDATECHECKER_LIBS)
