include $(top_srcdir)/build/vars.mk

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

spellcheck_la_LIBADD = \
	$(COMMON_LIBS) \
	$(ENCHANT_LIBS)
