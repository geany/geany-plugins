AC_DEFUN([GP_CHECK_LINEOPERATIONS],
[
    GP_ARG_DISABLE([LineOperations], [auto])
    GP_COMMIT_PLUGIN_STATUS([LineOperations])
    AC_CONFIG_FILES([
        lineoperations/Makefile
        lineoperations/src/Makefile
    ])
])
