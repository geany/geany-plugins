include $(top_srcdir)/build/vars.build.mk

if ENABLE_CODENAV
geanyplugins_LTLIBRARIES = codenav.la
else
EXTRA_LTLIBRARIES = codenav.la
endif

codenav_la_SOURCES = \
	codenavigation.c \
	codenavigation.h \
	goto_file.c \
	goto_file.h \
	switch_head_impl.c \
	switch_head_impl.h \
	utils.c \
	utils.h

codenav_la_LIBADD = $(COMMONLIBS)

