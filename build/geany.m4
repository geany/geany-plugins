AC_DEFUN([GP_CHECK_GEANY],
[
    AC_REQUIRE([PKG_PROG_PKG_CONFIG])
    AC_ARG_WITH([geany-libdir],
        AC_HELP_STRING([--with-geany-libdir=PATH],
            [Set Geany's installation libdir [[default=auto]]]),
        [geany_libdir=${withval}],
        [geany_libdir=$(${PKG_CONFIG} --variable=libdir geany)])

    export PKG_CONFIG_PATH="$geany_libdir/pkgconfig:$PKG_CONFIG_PATH"

    PKG_CHECK_MODULES([GEANY], [geany >= $1])
    geanypluginsdir=$geany_libdir/geany
    geanyversion=$(${PKG_CONFIG} --modversion geany)
    AC_SUBST([geanypluginsdir])

    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH#*:}"
    test -z "${PKG_CONFIG_PATH}" && unset PKG_CONFIG_PATH
])
