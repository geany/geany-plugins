include $(top_srcdir)/build/vars.build.mk
plugin = vimode

geanyplugins_LTLIBRARIES = vimode.la

noinst_PROGRAMS = viw

vi_srcs = \
	vi.h \
	vi.c \
	keypress.h \
	keypress.c \
	utils.h \
	utils.c \
	sci.h \
	sci.c \
	context.h \
	cmd-params.h \
	cmd-params.c \
	cmd-runner.h \
	cmd-runner.c \
	excmd-params.h \
	excmd-runner.h \
	excmd-runner.c \
	excmd-prompt.h \
	excmd-prompt.c \
	cmds/motion.h \
	cmds/motion.c \
	cmds/txtobjs.h \
	cmds/txtobjs.c \
	cmds/changemode.h \
	cmds/changemode.c \
	cmds/special.h \
	cmds/special.c \
	cmds/edit.h \
	cmds/edit.c \
	excmds/excmds.h \
	excmds/excmds.c

vimode_la_SOURCES = \
	backends/backend-geany.c \
	$(vi_srcs)

viw_SOURCES = \
	backends/backend-viw.c \
	$(vi_srcs)
    
vimode_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Vimode\"
vimode_la_CFLAGS = $(AM_CFLAGS)
vimode_la_LIBADD = $(COMMONLIBS)

viw_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Vimode\"
viw_CFLAGS = $(AM_CFLAGS)
viw_LDADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
