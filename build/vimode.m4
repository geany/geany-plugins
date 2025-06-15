AC_DEFUN([GP_CHECK_VIMODE],
[
    GP_ARG_DISABLE([Vimode], [auto])
    GP_COMMIT_PLUGIN_STATUS([Vimode])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
