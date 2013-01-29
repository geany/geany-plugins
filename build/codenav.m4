AC_DEFUN([GP_CHECK_CODENAV],
[
    GP_ARG_DISABLE([CodeNav], [yes])
    GP_COMMIT_PLUGIN_STATUS([CodeNav])
    AC_CONFIG_FILES([
        codenav/Makefile
        codenav/src/Makefile
    ])
])
