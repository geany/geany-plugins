include $(top_srcdir)/build/vars.build.mk

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

geanyvc_la_LIBADD = \
	$(GTKSPELL_LIBS) \
	$(COMMONLIBS)

check_LTLIBRARIES = libgeanyvc-test.la

libgeanyvc_test_la_SOURCES = \
	utils.c
libgeanyvc_test_la_CFLAGS = $(AM_CFLAGS) -DUNITTESTS
libgeanyvc_test_la_LDFLAGS = -avoid-version $(GP_LDFLAGS)
libgeanyvc_test_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
