AC_DEFUN([GP_CHECK_GEANYINSERTNUM],
[
    GP_ARG_DISABLE([GeanyInsertNum], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyInsertNum])
    AC_CONFIG_FILES([
        geanyinsertnum/Makefile
        geanyinsertnum/src/Makefile
    ])
])
