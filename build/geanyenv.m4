AC_DEFUN([GP_CHECK_GEANYENV],
[
    GP_ARG_DISABLE([Geanyenv], [auto])
    GP_COMMIT_PLUGIN_STATUS([Geanyenv])
    AC_CONFIG_FILES([
        geanyenv/Makefile
        geanyenv/src/Makefile
    ])
])
