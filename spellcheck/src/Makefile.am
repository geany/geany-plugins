include $(top_srcdir)/build/vars.build.mk

if ENABLE_SPELLCHECK
geanyplugins_LTLIBRARIES = spellcheck.la
else
EXTRA_LTLIBRARIES = spellcheck.la
endif

spellcheck_la_SOURCES = \
	scplugin.h \
	speller.h \
	gui.h \
	gui.c \
	speller.c \
	scplugin.c

spellcheck_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ENCHANT_CFLAGS)

spellcheck_la_LIBADD = \
	$(COMMONLIBS) \
	$(ENCHANT_LIBS)
