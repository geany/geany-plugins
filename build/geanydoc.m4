AC_DEFUN([GP_CHECK_GEANYDOC],
[
    GP_ARG_DISABLE([GeanyDoc], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyDoc])
    GP_COMMIT_PLUGIN_STATUS([GeanyDoc])
    AC_CONFIG_FILES([
        geanydoc/Makefile
        geanydoc/src/Makefile
        geanydoc/tests/Makefile
    ])
])
