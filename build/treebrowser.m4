AC_DEFUN([GP_CHECK_TREEBROWSER],
[
    GP_STATUS_PLUGIN_ADD([Treebrowser], [yes])
    AC_CONFIG_FILES([
        treebrowser/Makefile
        treebrowser/src/Makefile
    ])
])
