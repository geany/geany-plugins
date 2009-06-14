include $(top_srcdir)/build/vars.mk

geanyplugins_LTLIBRARIES = geanygdb.la

geanygdb_la_SOURCES = \
	gdb-io.h \
	gdb-io-priv.h \
	gdb-lex.h \
	gdb-ui.h \
	gdb-ui-envir.c \
	geanydebug.c \
	gdb-io-break.c \
	gdb-lex.c \
	gdb-ui-frame.c \
	gdb-ui-main.c \
	gdb-io-frame.c \
	gdb-io-run.c \
	ttyhelper.c \
	gdb-io-read.c \
	gdb-ui-break.c \
	gdb-io-envir.c \
	gdb-io-stack.c \
	gdb-ui-locn.c

geanygdb_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GEANY_CFLAGS)

geanygdb_la_LIBADD = \
	$(GEANY_LIBS)

geanygdb_la_LDFLAGS = -module -avoid-version
