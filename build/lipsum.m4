AC_DEFUN([GP_CHECK_LIPSUM],
[
    GP_ARG_DISABLE([Lipsum], [auto])
    GP_COMMIT_PLUGIN_STATUS([Lipsum])
    AC_CONFIG_FILES([
        lipsum/Makefile
        lipsum/src/Makefile
    ])
])
