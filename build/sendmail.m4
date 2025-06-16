AC_DEFUN([GP_CHECK_SENDMAIL],
[
    GP_ARG_DISABLE([Sendmail], [yes])
    GP_COMMIT_PLUGIN_STATUS([Sendmail])
    AC_CONFIG_FILES([
        sendmail/Makefile
        sendmail/src/Makefile
    ])
])
