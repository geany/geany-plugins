AC_DEFUN([GP_CHECK_GEANY],
[
    AC_REQUIRE([PKG_PROG_PKG_CONFIG])
    AC_ARG_WITH([geany-prefix],
        AC_HELP_STRING([--with-geany-prefix=PATH],
            [Set Geany's installation prefix [[default=auto]]]),
        [geany_prefix=${withval}],
        [geany_prefix=""])

    OLD_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
    test -z $geany_prefix || \
        export PKG_CONFIG_PATH="$geany_prefix/lib/pkgconfig:$PKG_CONFIG_PATH"

    PKG_CHECK_MODULES([GEANY], [geany >= $1])
    geanypluginsdir=$($PKG_CONFIG --variable=libdir)/geany
    AC_SUBST([geanypluginsdir])

    test -z $geany_prefix || \
        PKG_CONFIG_PATH="$OLD_PKG_CONFIG_PATH"

])
