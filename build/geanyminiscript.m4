AC_DEFUN([GP_CHECK_GEANYMINISCRIPT],
[
    GP_ARG_DISABLE([GeanyMiniScript], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyMiniScript])
    AC_CONFIG_FILES([
        geanyminiscript/Makefile
        geanyminiscript/src/Makefile
    ])
])
