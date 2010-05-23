include $(top_srcdir)/build/vars.build.mk
plugin = geanygendoc

if ENABLE_GEANYGENDOC
geanyplugins_LTLIBRARIES = geanygendoc.la
else
EXTRA_LTLIBRARIES = geanygendoc.la
endif

geanygendoc_la_SOURCES = ggd.c \
                         ggd-doc-setting.c \
                         ggd-doc-type.c \
                         ggd-file-type.c \
                         ggd-file-type-loader.c \
                         ggd-file-type-manager.c \
                         ggd-tag-utils.c \
                         ggd-options.c \
                         ggd-plugin.c \
                         ggd-utils.c \
                         ggd-widget-frame.c \
                         ggd-widget-doctype-selector.c
noinst_HEADERS         = ggd.h \
                         ggd-doc-setting.h \
                         ggd-doc-type.h \
                         ggd-file-type.h \
                         ggd-file-type-loader.h \
                         ggd-file-type-manager.h \
                         ggd-tag-utils.h \
                         ggd-macros.h \
                         ggd-options.h \
                         ggd-plugin.h \
                         ggd-utils.h \
                         ggd-widget-frame.h \
                         ggd-widget-doctype-selector.h
geanygendoc_la_CPPFLAGS= -DG_LOG_DOMAIN=\"GeanyGenDoc\"
geanygendoc_la_CFLAGS  = $(AM_CFLAGS) \
                         @GLIB_CFLAGS@ \
                         @GIO_CFLAGS@ \
                         @GTK_CFLAGS@ \
                         @CTPL_CFLAGS@
geanygendoc_la_LIBADD  = $(COMMONLIBS) \
                         @GLIB_LIBS@ \
                         @GIO_LIBS@ \
                         @GTK_LIBS@ \
                         @CTPL_LIBS@
