include $(top_srcdir)/build/vars.build.mk
plugin = incdec

geanyplugins_LTLIBRARIES = incdec.la


incdec_la_SOURCES  = incdec-plugin.c
incdec_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -DG_LOG_DOMAIN=\"IncDec\"
incdec_la_CFLAGS   = $(AM_CFLAGS) \
                        $(INCDEC_CFLAGS)
incdec_la_LIBADD   = $(COMMONLIBS) \
                        $(INCDEC_LIBS)


include $(top_srcdir)/build/cppcheck.mk
