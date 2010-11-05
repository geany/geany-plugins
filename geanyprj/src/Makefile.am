include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYPRJ
geanyplugins_LTLIBRARIES = geanyprj.la
else
EXTRA_LTLIBRARIES = geanyprj.la
endif
geanyprj_la_SOURCES = geanyprj.c \
	geanyprj.h \
	menu.c \
	project.c \
	sidebar.c \
	utils.c \
	xproject.c

geanyprj_la_CFLAGS = $(AM_CFLAGS)
geanyprj_la_LIBADD = $(COMMONLIBS)
