AC_DEFUN([GP_CHECK_OVERVIEW],
[
    GP_ARG_DISABLE([overview], [auto])

    GP_COMMIT_PLUGIN_STATUS([Overview])

    AC_CONFIG_FILES([
        overview/Makefile
        overview/data/Makefile
        overview/overview/Makefile
    ])
])
