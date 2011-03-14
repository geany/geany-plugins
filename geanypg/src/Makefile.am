include $(top_srcdir)/build/vars.build.mk

if ENABLE_GEANYPG
geanyplugins_LTLIBRARIES = geanypg.la
else
EXTRA_LTLIBRARIES = geanypg.la
endif

geanypg_la_SOURCES = \
	aux.c \
	encrypt_cb.c \
	key_selection_dialog.c \
	sign_cb.c \
	verify_cb.c \
	decrypt_cb.c \
	geanypg.c \
	geanypg.h \
	pinentry.c \
	verify_aux.c

geanypg_la_LIBADD = \
	$(COMMON_LIBS) \
	$(GPGME_LIBS)

geanypg_la__CFLAGS = \
	$(AM_CFLAGS) \
	$(GPGME_CFLAGS)

include $(top_srcdir)/build/cppcheck.mk
