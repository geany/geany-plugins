AC_DEFUN([GP_CHECK_GEANYEXTRASEL],
[
    GP_ARG_DISABLE([GeanyExtraSel], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyExtraSel])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
