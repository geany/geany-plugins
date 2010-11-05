AC_DEFUN([GP_CHECK_GEANYEXTRASEL],
[
    GP_ARG_DISABLE([GeanyExtraSel], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyExtraSel], [$enable_geanyextrasel])
    AC_CONFIG_FILES([
        geanyextrasel/Makefile
        geanyextrasel/src/Makefile
    ])
])
