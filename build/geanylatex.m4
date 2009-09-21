AC_DEFUN([GP_CHECK_GEANYLATEX],
[
    GP_STATUS_PLUGIN_ADD([GeanyLaTeX], [yes])
    AC_CONFIG_FILES([
        geanylatex/Makefile
        geanylatex/src/Makefile
    ])
])
