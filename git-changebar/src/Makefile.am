include $(top_srcdir)/build/vars.build.mk
plugin = git-changebar

geanyplugins_LTLIBRARIES = git-changebar.la

git_changebar_la_SOURCES  = gcb-plugin.c
git_changebar_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GitChangeBar\"
git_changebar_la_CFLAGS   = $(AM_CFLAGS) $(GITCHANGEBAR_CFLAGS)
git_changebar_la_LIBADD   = $(COMMONLIBS) \
                            $(GITCHANGEBAR_LIBS)

include $(top_srcdir)/build/cppcheck.mk
