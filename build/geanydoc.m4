AC_DEFUN([GP_CHECK_GEANYDOC],
[
    GP_STATUS_PLUGIN_ADD([GeanyDoc], [yes])
    AC_CONFIG_FILES([
        geanydoc/Makefile
        geanydoc/src/Makefile
	geanydoc/tests/Makefile
    ])
])
