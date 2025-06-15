AC_DEFUN([GP_CHECK_CODENAV],
[
    GP_ARG_DISABLE([CodeNav], [yes])
    GP_COMMIT_PLUGIN_STATUS([CodeNav])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
