AC_DEFUN([GP_CHECK_MULTITERM],
[
    GP_ARG_DISABLE([multiterm], [auto])

    GP_CHECK_PLUGIN_VALA([multiterm])
    GP_CHECK_PLUGIN_DEPS([multiterm], [MULTITERM], [gtk+-2.0 vte])
    GP_COMMIT_PLUGIN_STATUS([MultiTerm])

    AC_CONFIG_FILES([
        multiterm/Makefile
        multiterm/src/Makefile
    ])
])
