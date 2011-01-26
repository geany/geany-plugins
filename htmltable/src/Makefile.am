include $(top_srcdir)/build/vars.build.mk

if ENABLE_HTMLTABLE
geanyplugins_LTLIBRARIES = htmltable.la
else
EXTRA_LTLIBRARIES = htmltable.la
endif

htmltable_la_SOURCES = htmltable.c
htmltable_la_LIBADD = $(COMMONLIBS)
