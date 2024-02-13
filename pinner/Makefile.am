include $(top_srcdir)/build/vars.build.mk

plugin = pinner

geanyplugins_LTLIBRARIES = pinner.la

pinner_la_SOURCES = \
	pinner.c

pinner_la_CPPFLAGS = $(AM_CPPFLAGS) \
						 -DG_LOG_DOMAIN=\"Pinner\"

pinner_la_CFLAGS = \
	$(AM_CFLAGS) \
	-fsanitize=address,undefined

pinner_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-fsanitize=address,undefined

pinner_la_LIBADD = \
	$(COMMONLIBS)

AM_CPPCHECKFLAGS = -DSCE_PAS_DEFAULT=0
include $(top_srcdir)/build/cppcheck.mk
