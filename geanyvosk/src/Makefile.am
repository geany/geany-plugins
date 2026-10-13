include $(top_srcdir)/build/vars.build.mk
plugin = geanyvosk

geanyplugins_LTLIBRARIES = geanyvosk.la

geanyvosk_la_SOURCES  = geanyvosk.c
geanyvosk_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanyvosk_la_CFLAGS   = $(AM_CFLAGS) $(VOSK_CFLAGS) $(ALSA_CFLAGS)
geanyvosk_la_LIBADD   = $(COMMONLIBS) $(VOSK_LIBS) $(ALSA_LIBS)
geanyvosk_la_LDFLAGS  = $(AM_LDFLAGS)

include $(top_srcdir)/build/cppcheck.mk
