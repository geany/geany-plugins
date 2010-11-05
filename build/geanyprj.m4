AC_DEFUN([GP_CHECK_GEANYPRJ],
[
    GP_ARG_DISABLE([GeanyPrj], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyPrj], [$enable_geanyprj])
    AC_CONFIG_FILES([
        geanyprj/Makefile
        geanyprj/src/Makefile
	geanyprj/tests/Makefile
    ])
])
