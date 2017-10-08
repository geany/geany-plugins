AC_DEFUN([GP_CHECK_UTILS],
[
    GP_ARG_DISABLE([Utils], [auto])
    GP_COMMIT_PLUGIN_STATUS([Utils])
    AC_CONFIG_FILES([
        utils/Makefile
        utils/src/Makefile
    ])
])
