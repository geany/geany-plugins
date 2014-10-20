include $(top_srcdir)/build/vars.auxfiles.mk
include $(top_srcdir)/build/vars.build.mk

plugin = geanyprj

geanyplugins_LTLIBRARIES = geanyprj.la

geanyprj_la_SOURCES = \
	src/geanyprj.c \
	src/geanyprj.h \
	src/menu.c \
	src/project.c \
	src/sidebar.c \
	src/utils.c \
	src/xproject.c

geanyprj_la_CFLAGS = $(AM_CFLAGS)
geanyprj_la_LIBADD = $(COMMONLIBS)

# unit tests
if UNITTESTS
TESTS=unittests
check_PROGRAMS=unittests
unittests_SOURCES = tests/unittests.c src/utils.c
unittests_CFLAGS  = $(GEANY_CFLAGS) -I$(srcdir)/src -DUNITTESTS
unittests_LDADD   = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@
endif

# static analysis
AM_CPPCHECKFLAGS = --suppress='constStatement:*'
AM_CPPCHECKFLAGS += --suppress='memleak:src/utils.c:72'

include $(top_srcdir)/build/cppcheck.mk
