include $(top_srcdir)/build/vars.build.mk
plugin = playdate

geanyplugins_LTLIBRARIES = playdate.la

playdate_la_SOURCES = \
	playdate.c \
	playdate.h \
	config.c \
	build.c \
	errors.c \
	tags.c \
	tags_data.h

playdate_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Playdate\"
playdate_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
