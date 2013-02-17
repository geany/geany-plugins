AC_DEFUN([GP_CHECK_TABLECONVERT],
[
    GP_ARG_DISABLE([Tableconvert], [yes])
    GP_COMMIT_PLUGIN_STATUS([Tableconvert])
    AC_CONFIG_FILES([
        tableconvert/Makefile
        tableconvert/src/Makefile
    ])
])
