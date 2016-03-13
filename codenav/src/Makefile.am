include $(top_srcdir)/build/vars.build.mk
plugin = codenav

geanyplugins_LTLIBRARIES = codenav.la

codenav_la_SOURCES = \
	codenavigation.c \
	codenavigation.h \
	goto_file.c \
	goto_file.h \
	switch_head_impl.c \
	switch_head_impl.h \
	utils.c \
	utils.h

codenav_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"CodeNavigation\"
codenav_la_LIBADD = $(COMMONLIBS)


include $(top_srcdir)/build/cppcheck.mk
