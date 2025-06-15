AC_DEFUN([GP_CHECK_GEANYDOC],
[
    GP_ARG_DISABLE([GeanyDoc], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyDoc])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
