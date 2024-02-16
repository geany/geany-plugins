include $(top_srcdir)/build/vars.build.mk

plugin = pinner

geanyplugins_LTLIBRARIES = pinner.la

pinner_la_SOURCES = pinner.c

pinner_la_CPPFLAGS = $(AM_CPPFLAGS) \
						 -DG_LOG_DOMAIN=\"Pinner\"

if MINGW
	ASAN_FLAG =
else
	ASAN_FLAG = -fsanitize=address,undefined
endif

pinner_la_CFLAGS = \
	$(AM_CFLAGS) $(ASAN_FLAG)

pinner_la_LDFLAGS = \
	$(AM_LDFLAGS) $(ASAN_FLAG)

pinner_la_LIBADD = \
	$(COMMONLIBS)

AM_CPPCHECKFLAGS = -DSCE_PAS_DEFAULT=0
include $(top_srcdir)/build/cppcheck.mk
