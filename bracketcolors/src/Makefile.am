include $(top_srcdir)/build/vars.build.mk
plugin = bracketcolors

geanyplugins_LTLIBRARIES = bracketcolors.la

bracketcolors_srcs = \
    bracketcolors.cc \
    BracketMap.cc \
    BracketMap.h \
    Utils.h \
    Utils.cc \
    Configuration.h \
    Configuration.cc

bracketcolors_la_SOURCES = $(bracketcolors_srcs)
bracketcolors_la_CXXFLAGS = $(AM_CXXFLAGS) $(AM_CFLAGS) -DG_LOG_DOMAIN=\"BracketColors\"
bracketcolors_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
