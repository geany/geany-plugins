AC_DEFUN([GP_CHECK_GEANYEXTRASEL],
[
    GP_STATUS_PLUGIN_ADD([GeanyExtraSel], [yes])
    AC_CONFIG_FILES([
        geanyextrasel/Makefile
        geanyextrasel/src/Makefile
    ])
])
