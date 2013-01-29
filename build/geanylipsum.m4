AC_DEFUN([GP_CHECK_GEANYLIPSUM],
[
    GP_ARG_DISABLE([GeanyLipsum], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyLipsum])
    GP_COMMIT_PLUGIN_STATUS([GeanyLipsum])
    AC_CONFIG_FILES([
        geanylipsum/Makefile
        geanylipsum/src/Makefile
    ])
])
