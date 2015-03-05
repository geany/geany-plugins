if UNITTESTS
include $(top_srcdir)/build/vars.build.mk
TESTS=unittests
check_PROGRAMS=unittests
unittests_SOURCES = unittests.c
unittests_CFLAGS  = $(GEANY_CFLAGS) -DUNITTESTS
unittests_LDADD   = @GEANY_LIBS@ $(INTLLIBS) @CHECK_LIBS@ \
                    ../src/libgeanyvc-test.la
endif
