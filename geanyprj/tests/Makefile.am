if UNITTESTS
include $(top_srcdir)/build/vars.build.mk
TESTS=unittests
check_PROGRAMS=unittests
unittests_SOURCES = unittests.c
unittests_CFLAGS  = $(GEANY_CFLAGS) -I$(srcdir)/../src -DUNITTESTS
unittests_LDADD   = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@ \
                    ../src/libgeanyprj-test.la
endif
