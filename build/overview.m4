AC_DEFUN([GP_CHECK_OVERVIEW],
[
    GP_ARG_DISABLE([overview], [auto])

    GP_COMMIT_PLUGIN_STATUS([Overview])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
