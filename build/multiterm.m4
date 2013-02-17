AC_DEFUN([GP_CHECK_MULTITERM],
[
    GP_ARG_DISABLE([multiterm], [auto])

    AS_IF([test "$enable_multiterm" != "no"],
    [
        dnl FIXME: if the C sources are present (e.g. in a release tarball),
        dnl        we don't actually need valac
        AM_PROG_VALAC([0.7.0])
        AS_IF([test "$VALAC" = ""],
              [AS_IF([test "$enable_multiterm" = "auto"],
                     [enable_multiterm=no],
                     [AC_MSG_ERROR([valac not found])])])
    ])
    GP_CHECK_PLUGIN_DEPS([multiterm], [MULTITERM], [gtk+-2.0 geany vte])
    GP_COMMIT_PLUGIN_STATUS([MultiTerm])

    AC_CONFIG_FILES([
        multiterm/Makefile
        multiterm/src/Makefile
    ])
])
