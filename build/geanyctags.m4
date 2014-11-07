AC_DEFUN([GP_CHECK_GEANYCTAGS],
[
    GP_ARG_DISABLE([GeanyCtags], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyCtags])
    AC_CONFIG_FILES([
        geanyctags/Makefile
        geanyctags/src/Makefile
    ])
])
