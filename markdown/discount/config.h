/*
 * In the upstream Discount tree, this file is auto-generated, for the
 * Geany plugin it's not.
 */
#ifndef DISCOUNT_CONFIG_H
#define DISCOUNT_CONFIG_H 1

#include <glib.h>

G_BEGIN_DECLS

#define OS_LINUX G_OS_UNIX
#define USE_DISCOUNT_DL 0
#define DWORD guint32
#define WORD guint16
#define BYTE guint8
#define HAVE_BASENAME 0
#define HAVE_LIBGEN_H 0
#define HAVE_PWD_H 0
#define HAVE_GETPWUID 0
#define HAVE_SRANDOM 0
#define INITRNG(x) srandom((unsigned int)x)
#define HAVE_BZERO 0
#define HAVE_RANDOM 0
#define COINTOSS() (random()&1)
#define HAVE_STRCASECMP 0
#define HAVE_STRNCASECMP 0
#define HAVE_FCHDIR 0
#define TABSTOP 4
#define HAVE_MALLOC_H 0
#define VERSION "2.1.3"

G_END_DECLS

#endif /* DISCOUNT_CONFIG_H */
