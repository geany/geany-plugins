AC_DEFUN([GP_CHECK_GEANYSENDMAIL],
[
    GP_ARG_DISABLE([GeanySendmail], [yes])
    GP_STATUS_PLUGIN_ADD([GeanySendmail], [$enable_geanysendmail])
    AC_CONFIG_FILES([
        geanysendmail/Makefile
        geanysendmail/src/Makefile
    ])
])
