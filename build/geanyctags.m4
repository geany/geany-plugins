AC_DEFUN([GP_CHECK_GEANYCTAGS],
[
    GP_ARG_DISABLE([GeanyCtags], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyCtags])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
