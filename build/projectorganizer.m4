AC_DEFUN([GP_CHECK_PROJECTORGANIZER],
[
    GP_ARG_DISABLE([ProjectOrganizer], [auto])
    GP_COMMIT_PLUGIN_STATUS([ProjectOrganizer])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
