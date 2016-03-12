include $(top_srcdir)/build/vars.build.mk
plugin = geanyvc

geanyplugins_LTLIBRARIES = geanyvc.la

geanyvc_la_SOURCES = \
	externdiff.c \
	geanyvc.c \
	utils.c \
	vc_bzr.c \
	vc_cvs.c \
	vc_git.c \
	vc_hg.c \
	vc_svk.c \
	vc_svn.c \
	geanyvc.h

geanyvc_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GTKSPELL_CFLAGS)

geanyvc_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyVC\"
geanyvc_la_LIBADD = \
	$(GTKSPELL_LIBS) \
	$(COMMONLIBS)

if UNITTESTS
TESTS = unittests
check_PROGRAMS = unittests
unittests_SOURCES = unittests.c utils.c
unittests_CFLAGS  = $(GEANY_CFLAGS) -DUNITTESTS
unittests_LDADD   = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@
endif

include $(top_srcdir)/build/cppcheck.mk
