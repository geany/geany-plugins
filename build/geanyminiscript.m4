AC_DEFUN([GP_CHECK_GEANYMINISCRIPT],
[
    GP_ARG_DISABLE([GeanyMiniScript], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyMiniScript])
    GP_COMMIT_PLUGIN_STATUS([GeanyMiniScript])
    AC_CONFIG_FILES([
        geanyminiscript/Makefile
        geanyminiscript/src/Makefile
    ])
])
