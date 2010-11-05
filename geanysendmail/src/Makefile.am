include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYSENDMAIL
geanyplugins_LTLIBRARIES = geanysendmail.la
else
EXTRA_LTLIBRARIES = geanysendmail.la
endif

geanysendmail_la_SOURCES = geanysendmail.c icon.h
geanysendmail_la_LIBADD = $(COMMONLIBS)
