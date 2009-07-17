AC_DEFUN([GP_CHECK_GEANYGDB],
[
    GP_STATUS_PLUGIN_ADD([GeanyGDB], [yes])
    AC_CONFIG_FILES([
        geanygdb/Makefile
        geanygdb/src/Makefile
        geanygdb/tests/Makefile
    ])
])
