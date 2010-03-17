AC_DEFUN([GP_CHECK_GEANYINSERTNUM],
[
    GP_STATUS_PLUGIN_ADD([GeanyInsertNum], [yes])
    AC_CONFIG_FILES([
        geanyinsertnum/Makefile
        geanyinsertnum/src/Makefile
    ])
])
