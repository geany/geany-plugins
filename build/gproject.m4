AC_DEFUN([GP_CHECK_GPROJECT],
[
    GP_ARG_DISABLE([GProject], [auto])
    GP_COMMIT_PLUGIN_STATUS([GProject])
    AC_CONFIG_FILES([
        gproject/Makefile
        gproject/src/Makefile
        gproject/icons/Makefile
    ])
])
