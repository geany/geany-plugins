include $(top_srcdir)/build/vars.build.mk
plugin = pairtaghighlighter

geanyplugins_LTLIBRARIES = pairtaghighlighter.la

pairtaghighlighter_la_SOURCES = pair_tag_highlighter.c

pairtaghighlighter_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"PairTagHighlighter\"
pairtaghighlighter_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
