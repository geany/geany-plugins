## Makefile.am -- Process this file with automake to produce Makefile.in
#SUBDIRS = tests

INCLUDES = @CHECK_CFLAGS@ @GEANY_CFLAGS@ -DUNITTESTS

if UNITTESTS
TESTS=unittests
noinst_PROGRAMS=unittests
unittests_SOURCES = unittests.c
unittests_LDADD  = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@
endif
