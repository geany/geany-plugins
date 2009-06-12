
geanylibdir = $(libdir)/geany
geanylib_LTLIBRARIES = geanygdb.la
geanygdb_la_SOURCES = gdb-io-break.c \
    gdb-io-envir.c \
    gdb-io-frame.c \
    gdb-io-priv.h \
    gdb-io-read.c \
    gdb-io-run.c \
    gdb-io-stack.c \
    gdb-io.h \
    gdb-lex.c \
    gdb-lex.h \
    gdb-ui-break.c \
    gdb-ui-envir.c \
    gdb-ui-frame.c \
    gdb-ui-locn.c \
    gdb-ui-main.c \
    gdb-ui.h \
    geanydebug.c

geanygdb_la_LDFLAGS = -module -avoid-version
geanygdb_la_LIBADD  = @GEANY_LIBS@  $(INTLLIBS)

geanylib_PROGRAMS = ttyhelper
ttyhelper_SOURCES = ttyhelper.c



AM_CFLAGS = @GEANY_CFLAGS@ -DLOCALEDIR=\""$(localedir)"\" -DLIBDIR=\""$(libdir)"\"

