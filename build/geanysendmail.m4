AC_DEFUN([GP_CHECK_GEANYSENDMAIL],
[
    GP_ARG_DISABLE([GeanySendmail], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanySendmail])
    AC_CONFIG_FILES([
        geanysendmail/Makefile
        geanysendmail/src/Makefile
    ])
])
