AC_DEFUN([GP_CHECK_GEANYPRJ],
[
    GP_ARG_DISABLE([GeanyPrj], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyPrj])
    AC_CONFIG_FILES([
        geanyprj/Makefile
        geanyprj/src/Makefile
    ])
])
