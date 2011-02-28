AC_INIT([geanypg], [0.1], [alves.h88@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# checking for gpgme
AM_PATH_GPGME()

# checking for Geany
PKG_CHECK_MODULES(GEANY, [geany >= 0.20])
AC_SUBST(GEANY_CFLAGS)
AC_SUBST(GEANY_LIBS)

libdir="`$PKG_CONFIG --variable=libdir geany`/geany"
datadir="`$PKG_CONFIG --variable=datadir geany`/geany/plugins/${PACKAGE_NAME}"

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
