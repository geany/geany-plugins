include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYGDB
geanygdbdir = $(libexecdir)/geany-plugins/geanygdb
geanyplugins_LTLIBRARIES = geanygdb.la
geanygdb_PROGRAMS = ttyhelper
else
EXTRA_LTLIBRARIES = geanygdb.la
EXTRA_PROGRAMS = ttyhelper
endif

geanygdb_la_SOURCES = \
	gdb-io.h \
	gdb-io-priv.h \
	gdb-lex.h \
	gdb-ui.h \
	gdb-ui-envir.c \
	geanygdb.c \
	gdb-io-break.c \
	gdb-lex.c \
	gdb-ui-frame.c \
	gdb-ui-main.c \
	gdb-io-frame.c \
	gdb-io-run.c \
	gdb-io-read.c \
	gdb-ui-break.c \
	gdb-io-envir.c \
	gdb-io-stack.c \
	gdb-ui-locn.c

geanygdb_la_CFLAGS = $(AM_CFLAGS) -DTTYHELPERDIR=\""$(geanygdbdir)"\"
geanygdb_la_LIBADD = $(COMMONLIBS)

ttyhelper_SOURCES = ttyhelper.c
