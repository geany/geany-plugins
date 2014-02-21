include $(top_srcdir)/build/vars.build.mk

geanyplugins_LTLIBRARIES = spellcheck.la

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

if HAVE_ENCHANT_1_5
spellcheck_la_CFLAGS += -DHAVE_ENCHANT_1_5
endif

spellcheck_la_LIBADD = \
	$(COMMONLIBS) \
	$(ENCHANT_LIBS)

AM_CPPCHECKFLAGS = -DSCE_PAS_DEFAULT=0
include $(top_srcdir)/build/cppcheck.mk
