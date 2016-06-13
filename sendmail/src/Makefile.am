include $(top_srcdir)/build/vars.build.mk
plugin = sendmail

geanyplugins_LTLIBRARIES = sendmail.la

sendmail_la_SOURCES = sendmail.c mail-icon.xpm
sendmail_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"SendMail\"
sendmail_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
