AC_DEFUN([GP_CHECK_NUMCONVERT],
[
    GP_ARG_DISABLE([numconvert], [auto])

    AS_IF([test "$enable_numconvert" != "no"],
    [
        dnl FIXME: if the C sources are present (e.g. in a release tarball),
        dnl        we don't actually need valac
        AM_PROG_VALAC([0.7.0])
        AS_IF([test "$VALAC" = ""],
              [AS_IF([test "$enable_numconvert" = "auto"],
                     [enable_numconvert=no],
                     [AC_MSG_ERROR([valac not found])])])
    ])
    GP_CHECK_PLUGIN_DEPS([numconvert], [NUMCONVERT], [gtk+-2.0 geany])
    GP_COMMIT_PLUGIN_STATUS([NumConvert])

    AC_CONFIG_FILES([
        numconvert/Makefile
        numconvert/src/Makefile
    ])
])
