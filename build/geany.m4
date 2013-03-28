AC_DEFUN([_GP_GEANY_LIBDIR],
[
    AC_REQUIRE([PKG_PROG_PKG_CONFIG])
    AC_ARG_WITH([geany-libdir],
        AC_HELP_STRING([--with-geany-libdir=PATH],
            [Set Geany's installation libdir [[default=auto]]]),
        [geany_libdir=${withval}],
        [geany_libdir=$(${PKG_CONFIG} --variable=libdir geany)])
])

dnl GP_GEANY_PKG_CONFIG_PATH_PUSH
dnl Updates PKG_CONFIG_PATH to include the appropriate directory to match
dnl --with-geany-libdir option.  This is useful when calling PKG_CONFIG on the
dnl geany package but should be avoided for any other package.
dnl Call GP_GEANY_PKG_CONFIG_PATH_POP to undo the action
AC_DEFUN([GP_GEANY_PKG_CONFIG_PATH_PUSH],
[
    AC_REQUIRE([_GP_GEANY_LIBDIR])
    export PKG_CONFIG_PATH="$geany_libdir/pkgconfig:$PKG_CONFIG_PATH"
])

dnl GP_GEANY_PKG_CONFIG_PATH_POP
dnl Undoes what GP_GEANY_PKG_CONFIG_PATH_PUSH did
AC_DEFUN([GP_GEANY_PKG_CONFIG_PATH_POP],
[
    AC_REQUIRE([_GP_GEANY_LIBDIR])
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH#*:}"
    test -z "${PKG_CONFIG_PATH}" && unset PKG_CONFIG_PATH
])

AC_DEFUN([GP_CHECK_GEANY],
[
    AC_REQUIRE([PKG_PROG_PKG_CONFIG])

    GP_GEANY_PKG_CONFIG_PATH_PUSH

    PKG_CHECK_MODULES([GEANY], [geany >= $1])
    geanypluginsdir=$geany_libdir/geany
    GEANY_VERSION=$(${PKG_CONFIG} --modversion geany)
    AC_SUBST([geanypluginsdir])
    AC_SUBST([GEANY_VERSION])

    GP_GEANY_PKG_CONFIG_PATH_POP
])
