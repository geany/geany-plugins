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

dnl _GP_GEANY_VERSION_MATCH(VERSION, [ACTION-IF-OK], [ACTION-IF-TOO-OLD])
AC_DEFUN([_GP_GEANY_VERSION_MATCH],
[
    AC_REQUIRE([GP_CHECK_GEANY])dnl for GEANY_VERSION
    AS_VERSION_COMPARE([$GEANY_VERSION], [$1], [$3], [$2], [$2])
])

dnl _GP_GEANY_API_VERSION
AC_DEFUN([_GP_GEANY_API_VERSION],
[
    AC_REQUIRE([GP_CHECK_GEANY])dnl for GEANY_CFLAGS

    GEANY_API_VERSION=0

    AC_LANG_PUSH([C])
    gp_geany_api_version_CPPFLAGS="$CPPFLAGS"
    dnl FIXME: we should rather use CPPFLAGS but pkg-config doesn't have it --
    dnl        so just hope the flags are valid preprocessor ones
    CPPFLAGS="$CPPFLAGS $GEANY_CFLAGS"
    AC_MSG_CHECKING([for Geany API version])
    AC_PREPROC_IFELSE(
        [AC_LANG_SOURCE([[#include <geanyplugin.h>
GEANY_API_VERSION]])],
        [GEANY_API_VERSION="$(tail -n 1 conftest.i)"
         AC_MSG_RESULT([$GEANY_API_VERSION])],
        [dnl well, for some reason the real check failed, so try and guess.
         dnl see https://wiki.geany.org/plugins/development/api-versions
         _GP_GEANY_VERSION_MATCH([0.21], [GEANY_API_VERSION=211])
         _GP_GEANY_VERSION_MATCH([1.22], [GEANY_API_VERSION=215])
         _GP_GEANY_VERSION_MATCH([1.23], [GEANY_API_VERSION=216])
         _GP_GEANY_VERSION_MATCH([1.24], [GEANY_API_VERSION=217])
         AC_MSG_RESULT([$GEANY_API_VERSION (guessed)])
         AC_MSG_WARN([The check for determining the Geany API version failed.])
         AC_MSG_WARN([A fallback was used, guessing the value instead, based])
         AC_MSG_WARN([on the Geany version itself.  This is not very reliable])
         AC_MSG_WARN([and should not happen.  Please report a bug!])])
    CPPFLAGS="$gp_geany_api_version_CPPFLAGS"
    AC_LANG_POP([C])
])

dnl _GP_CHECK_PLUGIN_GEANY_VERSION(PluginName, VERSION-REQ, VERSION, VERSION-DESC)
dnl Checks whether VERSION is at least VERSION-REQ, and error out/disables
dnl plugins appropriately depending on enable_$plugin
AC_DEFUN([_GP_CHECK_PLUGIN_GEANY_VERSION],
[
    AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = no],[],
          [AC_MSG_CHECKING([whether the Geany $4 in use is compatible with plugin $1])
           AS_VERSION_COMPARE([$3], [$2],
                              [AC_MSG_RESULT([no])
                               AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = yes],
                                     [AC_MSG_ERROR([$1 is not compatible with the Geany $4 in use])],
                                     [test "$m4_tolower(AS_TR_SH(enable_$1))" = auto],
                                     [m4_tolower(AS_TR_SH(enable_$1))=no])],
                              [AC_MSG_RESULT([yes])],
                              [AC_MSG_RESULT([yes])])])
])

dnl GP_CHECK_PLUGIN_GEANY_VERSION(PluginName, GEANY-VERSION)
dnl Checks whether Geany's version is at least GEANY-VERSION, and error
dnl out/disables plugins appropriately depending on enable_$plugin
AC_DEFUN([GP_CHECK_PLUGIN_GEANY_VERSION],
[
    AC_REQUIRE([GP_CHECK_GEANY])dnl for GEANY_VERSION
    _GP_CHECK_PLUGIN_GEANY_VERSION([$1], [$2], [$GEANY_VERSION], [version])
])

dnl GP_CHECK_PLUGIN_GEANY_API_VERSION(PluginName, GEANY-API-VERSION)
dnl Checks whether Geany's API version is at least GEANY-API-VERSION, and error
dnl out/disables plugins appropriately depending on enable_$plugin
AC_DEFUN([GP_CHECK_PLUGIN_GEANY_API_VERSION],
[
    AC_REQUIRE([_GP_GEANY_API_VERSION])
    _GP_CHECK_PLUGIN_GEANY_VERSION([$1], [$2], [$GEANY_API_VERSION], [API version])
])
