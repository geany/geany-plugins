AC_DEFUN([GP_CHECK_GEANYINSERTNUM],
[
    GP_ARG_DISABLE([GeanyInsertNum], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyInsertNum])
    GP_COMMIT_PLUGIN_STATUS([GeanyInsertNum])
    AC_CONFIG_FILES([
        geanyinsertnum/Makefile
        geanyinsertnum/src/Makefile
    ])
])
