include $(top_srcdir)/build/vars.build.mk
plugin = codesubmit

codesubmit_LTLIBRARIES = codesubmit.la

codesubmit_la_SOURCES = codesubmit.c
codesubmit_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DGEANY_VERSION=\"$(GEANY_VERSION)\" \
	-DG_LOG_DOMAIN=\"CodeSubmit\"
codesubmit_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GENIUSPASTE_CFLAGS)
codesubmit_la_LIBADD = \
	$(COMMONLIBS) \
	$(GENIUSPASTE_LIBS)

include $(top_srcdir)/build/cppcheck.mk
