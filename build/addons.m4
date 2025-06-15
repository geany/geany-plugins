AC_DEFUN([GP_CHECK_ADDONS],
[
    GP_ARG_DISABLE([Addons], [auto])
    GP_COMMIT_PLUGIN_STATUS([Addons])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
