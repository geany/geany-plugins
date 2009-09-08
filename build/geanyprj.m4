AC_DEFUN([GP_CHECK_GEANYPRJ],
[
    GP_STATUS_PLUGIN_ADD([GeanyPrj], [yes])
    AC_CONFIG_FILES([
        geanyprj/Makefile
        geanyprj/src/Makefile
	geanyprj/tests/Makefile
    ])
])
