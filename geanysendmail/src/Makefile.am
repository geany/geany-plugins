include $(top_srcdir)/build/vars.build.mk
plugin = geanysendmail

geanyplugins_LTLIBRARIES = geanysendmail.la

geanysendmail_la_SOURCES = geanysendmail.c mail-icon.xpm
geanysendmail_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanySendMail\"
geanysendmail_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
