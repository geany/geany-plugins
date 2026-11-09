AC_DEFUN([GP_CHECK_GEANYSERVERS],
[
    GP_ARG_DISABLE([Geanyservers], [auto])

    GP_COMMIT_PLUGIN_STATUS([Geanyservers])

    AC_CONFIG_FILES([
        geanyservers/Makefile
        geanyservers/src/Makefile
    ])
])
