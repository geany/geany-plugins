AC_DEFUN([GP_CHECK_GEANYLATEX],
[
    GP_ARG_DISABLE([GeanyLaTeX], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyLaTeX])
    GP_COMMIT_PLUGIN_STATUS([GeanyLaTeX])
    AC_CONFIG_FILES([
        geanylatex/Makefile
        geanylatex/doc/Makefile
        geanylatex/src/Makefile
    ])
])
