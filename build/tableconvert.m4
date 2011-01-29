AC_DEFUN([GP_CHECK_TABLECONVERT],
[
    GP_ARG_DISABLE([Tableconvert], [yes])
    GP_STATUS_PLUGIN_ADD([Tableconvert], [$enable_tableconvert])
    AC_CONFIG_FILES([
        tableconvert/Makefile
        tableconvert/src/Makefile
    ])
])
