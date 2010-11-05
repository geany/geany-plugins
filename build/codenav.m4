AC_DEFUN([GP_CHECK_CODENAV],
[
    GP_ARG_DISABLE([CodeNav], [yes])
    GP_STATUS_PLUGIN_ADD([CodeNav], [$enable_codenav])
    AC_CONFIG_FILES([
        codenav/Makefile
        codenav/src/Makefile
    ])
])
