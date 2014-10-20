include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk

plugin = geanyvc

geanyplugins_LTLIBRARIES = geanyvc.la

geanyvc_la_SOURCES = \
	src/externdiff.c \
	src/geanyvc.c \
	src/utils.c \
	src/vc_bzr.c \
	src/vc_cvs.c \
	src/vc_git.c \
	src/vc_hg.c \
	src/vc_svk.c \
	src/vc_svn.c \
	src/geanyvc.h

geanyvc_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GTKSPELL_CFLAGS)

geanyvc_la_LIBADD = \
	$(GTKSPELL_LIBS) \
	$(COMMONLIBS)

# unit tests
if UNITTESTS
TESTS = unittests
check_PROGRAMS = unittests
unittests_SOURCES = tests/unittests.c src/utils.c
unittests_CFLAGS  = $(GEANY_CFLAGS) -DUNITTESTS
unittests_LDADD   = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@
endif

# static analysis
include $(top_srcdir)/build/cppcheck.mk
