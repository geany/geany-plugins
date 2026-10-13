AC_DEFUN([GP_CHECK_GEANYPROGRESS],
[
    GP_ARG_DISABLE([Geanyprogress], [auto])
    GP_COMMIT_PLUGIN_STATUS([Geanyprogress])
    AC_CONFIG_FILES([
        geanyprogress/Makefile
        geanyprogress/src/Makefile
    ])
])
