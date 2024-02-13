AC_DEFUN([GP_CHECK_PINNER],
[
    GP_ARG_DISABLE([pinner], [auto])
    GP_COMMIT_PLUGIN_STATUS([Pinner])

    AC_CONFIG_FILES([
        pinner/Makefile
    ])
])
