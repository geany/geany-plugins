AC_DEFUN([GP_CHECK_GEANYMINISCRIPT],
[
    GP_ARG_DISABLE([GeanyMiniScript], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyMiniScript], [$enable_geanyminiscript])
    AC_CONFIG_FILES([
        geanyminiscript/Makefile
        geanyminiscript/src/Makefile
    ])
])
