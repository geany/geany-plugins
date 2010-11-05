AC_DEFUN([GP_CHECK_GEANYINSERTNUM],
[
    GP_ARG_DISABLE([GeanyInsertNum], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyInsertNum], [$enable_geanyinsertnum])
    AC_CONFIG_FILES([
        geanyinsertnum/Makefile
        geanyinsertnum/src/Makefile
    ])
])
