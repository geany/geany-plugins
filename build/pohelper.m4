AC_DEFUN([GP_CHECK_POHELPER],
[
    GP_ARG_DISABLE([PoHelper], [yes])

    GP_COMMIT_PLUGIN_STATUS([PoHelper])

    AC_CONFIG_FILES([
        pohelper/Makefile
        pohelper/data/Makefile
        pohelper/src/Makefile
    ])
])
