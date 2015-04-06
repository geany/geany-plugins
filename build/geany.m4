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

dnl GP_CHECK_PLUGIN_GEANY_VERSION(PluginName, GEANY-VERSION)
dnl Checks whether Geany's version is at least GEANY-VERSION, and error
dnl out/disables plugins appropriately depending on enable_$plugin
AC_DEFUN([GP_CHECK_PLUGIN_GEANY_VERSION],
[
    AC_REQUIRE([GP_CHECK_GEANY])dnl for GEANY_VERSION

    AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = no],[],
          [AC_MSG_CHECKING([whether the Geany version in use is compatible with plugin $1])
           AS_VERSION_COMPARE([$GEANY_VERSION], [$2],
                              [AC_MSG_RESULT([no])
                               AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = yes],
                                     [AC_MSG_ERROR([$1 is not compatible with the Geany version in use])],
                                     [test "$m4_tolower(AS_TR_SH(enable_$1))" = auto],
                                     [m4_tolower(AS_TR_SH(enable_$1))=no])],
                              [AC_MSG_RESULT([yes])],
                              [AC_MSG_RESULT([yes])])])
])
