include $(top_srcdir)/build/vars.build.mk
plugin = updatechecker

geanyplugins_LTLIBRARIES = updatechecker.la

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

include $(top_srcdir)/build/cppcheck.mk
