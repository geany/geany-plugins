AC_DEFUN([GP_CHECK_GEANYEXTRASEL],
[
    GP_ARG_DISABLE([GeanyExtraSel], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyExtraSel])
    AC_CONFIG_FILES([
        geanyextrasel/Makefile
        geanyextrasel/src/Makefile
    ])
])
