AC_DEFUN([GP_CHECK_GEANYDOC],
[
    GP_ARG_DISABLE([GeanyDoc], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyDoc])
    AC_CONFIG_FILES([
        geanydoc/Makefile
        geanydoc/src/Makefile
        geanydoc/tests/Makefile
    ])
])
