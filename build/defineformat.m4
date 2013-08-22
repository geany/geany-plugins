AC_DEFUN([GP_CHECK_DEFINEFORMAT],
[
    GP_ARG_DISABLE([Defineformat], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([Defineformat])
    GP_COMMIT_PLUGIN_STATUS([Defineformat])
    AC_CONFIG_FILES([
        defineformat/Makefile
        defineformat/src/Makefile
    ])
])
