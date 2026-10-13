AC_DEFUN([GP_CHECK_LOCATE],
[
    GP_ARG_DISABLE([Locate], [auto])
    GP_COMMIT_PLUGIN_STATUS([Locate])
    AC_CONFIG_FILES([
        locate/Makefile
        locate/src/Makefile
    ])
])
