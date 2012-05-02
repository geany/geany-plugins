AC_DEFUN([GP_CHECK_GENIUSPASTE],
[
    GP_ARG_DISABLE([GeniusPaste], [auto])

    GP_CHECK_PLUGIN_DEPS([GeniusPaste], GENIUSPASTE,
                         [libsoup-2.4 >= 2.4.0])

    GP_STATUS_PLUGIN_ADD([GeniusPaste], [$enable_geniuspaste])

    AC_CONFIG_FILES([
        geniuspaste/Makefile
        geniuspaste/src/Makefile
    ])
])
