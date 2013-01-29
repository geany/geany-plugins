AC_DEFUN([GP_CHECK_GEANYLATEX],
[
    GP_ARG_DISABLE([GeanyLaTeX], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyLaTeX])
    AC_CONFIG_FILES([
        geanylatex/Makefile
        geanylatex/doc/Makefile
        geanylatex/src/Makefile
    ])
])
