include $(top_srcdir)/build/vars.build.mk

if ENABLE_GPROJECT
geanyplugins_LTLIBRARIES = gproject.la
else
EXTRA_LTLIBRARIES = gproject.la
endif
gproject_la_SOURCES = \
	gproject-main.c \
	gproject-project.h \
	gproject-project.c \
	gproject-sidebar.h \
	gproject-sidebar.c \
	gproject-utils.h \
	gproject-utils.c \
	gproject-menu.h \
	gproject-menu.c

gproject_la_CFLAGS = $(AM_CFLAGS)
gproject_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk

