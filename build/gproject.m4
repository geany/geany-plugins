AC_DEFUN([GP_CHECK_GPROJECT],
[
    GP_ARG_DISABLE([GProject], [yes])
    GP_STATUS_PLUGIN_ADD([GProject], [$enable_gproject])
    AC_CONFIG_FILES([
        gproject/Makefile
        gproject/src/Makefile
        gproject/icons/Makefile
    ])
])
