AC_DEFUN([GP_CHECK_SENDMAIL],
[
    GP_ARG_DISABLE([Sendmail], [yes])
    GP_COMMIT_PLUGIN_STATUS([Sendmail])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
