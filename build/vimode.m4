AC_DEFUN([GP_CHECK_VIMODE],
[
    GP_ARG_DISABLE([Vimode], [auto])
    GP_COMMIT_PLUGIN_STATUS([Vimode])
    AC_CONFIG_FILES([
        vimode/Makefile
        vimode/src/Makefile
    ])
])
