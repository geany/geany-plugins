AC_DEFUN([GP_CHECK_GENIUSPASTE],
[
    GP_ARG_DISABLE([GeniusPaste], [auto])

    GP_CHECK_PLUGIN_DEPS([GeniusPaste], GENIUSPASTE,
                         [libsoup-3.0])

    GP_COMMIT_PLUGIN_STATUS([GeniusPaste])

    AC_CONFIG_FILES([
        geniuspaste/Makefile
        geniuspaste/data/Makefile
        geniuspaste/src/Makefile
    ])
])
