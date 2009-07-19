AC_DEFUN([GP_CHECK_CODENAV],
[
    GP_STATUS_PLUGIN_ADD([CodeNav], [yes])
    AC_CONFIG_FILES([
        codenav/Makefile
        codenav/src/Makefile
    ])
])
