AC_DEFUN([GP_CHECK_MULTITERM],
[
    AM_PROG_VALAC([0.7.0])

    GP_ARG_DISABLE([multiterm], [auto])
    GP_CHECK_PLUGIN_DEPS([multiterm], [MULTITERM], [gtk+-2.0 geany vte])
    GP_STATUS_PLUGIN_ADD([MultiTerm], [$enable_multiterm])

    AC_CONFIG_FILES([
        multiterm/Makefile
        multiterm/src/Makefile
    ])
])
