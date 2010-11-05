AC_DEFUN([GP_CHECK_GEANYDOC],
[
    GP_ARG_DISABLE([GeanyDoc], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyDoc], [$enable_geanydoc])
    AC_CONFIG_FILES([
        geanydoc/Makefile
        geanydoc/src/Makefile
	geanydoc/tests/Makefile
    ])
])
