AC_DEFUN([GP_CHECK_GEANYSENDMAIL],
[
    GP_STATUS_PLUGIN_ADD([GeanySendmail], [yes])
    AC_CONFIG_FILES([
        geanysendmail/Makefile
        geanysendmail/src/Makefile
    ])
])
