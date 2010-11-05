AC_DEFUN([GP_CHECK_GEANYLIPSUM],
[
    GP_ARG_DISABLE([GeanyLipsum], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyLipsum], [$enable_geanylipsum])
    AC_CONFIG_FILES([
        geanylipsum/Makefile
        geanylipsum/src/Makefile
    ])
])
