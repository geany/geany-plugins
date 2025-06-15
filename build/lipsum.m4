AC_DEFUN([GP_CHECK_LIPSUM],
[
    GP_ARG_DISABLE([Lipsum], [auto])
    GP_COMMIT_PLUGIN_STATUS([Lipsum])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
