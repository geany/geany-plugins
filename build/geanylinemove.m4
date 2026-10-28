AC_DEFUN([GP_CHECK_GEANYLINEMOVE],
[
    GP_ARG_DISABLE([GeanyLineMove], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyLineMove])
    AC_CONFIG_FILES([
        geanylinemove/Makefile
        geanylinemove/src/Makefile
    ])
])
