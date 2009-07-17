AC_DEFUN([GP_CHECK_GEANYLIPSUM],
[
    GP_STATUS_PLUGIN_ADD([GeanyLipsum], [yes])
    AC_CONFIG_FILES([
        geanylipsum/Makefile
        geanylipsum/src/Makefile
    ])
])
