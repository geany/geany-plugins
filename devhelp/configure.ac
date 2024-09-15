#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([geany_devhelp], [0.1], [mbrush@leftclick.ca])
AC_CONFIG_SRCDIR([src/dhp-plugin.h])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([DEVHELP], [libdevhelp-3.0])
PKG_CHECK_MODULES(GEANY,geany)
PKG_CHECK_MODULES([GEANY_DEVHELP], [gtk+-3.0])


# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset strrchr strstr strtol])

AC_CONFIG_FILES([Makefile
                 data/Makefile
                 src/Makefile])
AC_OUTPUT
