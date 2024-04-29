include $(top_srcdir)/build/vars.build.mk
plugin = geanyprj

geanyplugins_LTLIBRARIES = geanyprj.la

geanyprj_la_SOURCES = geanyprj.c \
	geanyprj.h \
	menu.c \
	project.c \
	sidebar.c \
	utils.c \
	xproject.c

geanyprj_la_CFLAGS = $(AM_CFLAGS)
geanyprj_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyPrj\"
geanyprj_la_LIBADD = $(COMMONLIBS)

if UNITTESTS
TESTS = unittests
check_PROGRAMS = unittests
unittests_SOURCES = unittests.c utils.c
unittests_CFLAGS  = $(GEANY_CFLAGS) -DUNITTESTS
unittests_LDADD   = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@
endif

AM_CPPCHECKFLAGS = --suppress='constStatement:*'

include $(top_srcdir)/build/cppcheck.mk
