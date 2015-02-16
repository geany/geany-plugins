include $(top_srcdir)/build/vars.build.mk
plugin = git-ui

geanyplugins_LTLIBRARIES = git-ui.la

git_ui_la_SOURCES   = ggu-plugin.c
git_ui_la_CPPFLAGS  = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GitUI\" \
                      -DPLUGIN=\""$(plugin)"\"
git_ui_la_CFLAGS    = $(AM_CFLAGS) $(GITUI_CFLAGS)
git_ui_la_LIBADD    = $(COMMONLIBS) \
                      $(GITUI_LIBS)

include $(top_srcdir)/build/cppcheck.mk
