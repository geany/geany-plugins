include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYVC
geanyplugins_LTLIBRARIES = geanyvc.la
else
EXTRA_LTLIBRARIES = geanyvc.la
endif

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

include $(top_srcdir)/build/cppcheck.mk
