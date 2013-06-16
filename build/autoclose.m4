AC_DEFUN([GP_CHECK_AUTOCLOSE],
[
    GP_ARG_DISABLE([Autoclose], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([Autoclose])
    GP_COMMIT_PLUGIN_STATUS([Autoclose])
    AC_CONFIG_FILES([
        autoclose/Makefile
        autoclose/src/Makefile
    ])
])
