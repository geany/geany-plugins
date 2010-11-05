AC_DEFUN([GP_CHECK_GEANYLATEX],
[
    GP_ARG_DISABLE([GeanyLaTeX], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyLaTeX], [$enable_geanylatex])
    AC_CONFIG_FILES([
        geanylatex/Makefile
        geanylatex/src/Makefile
    ])
])
