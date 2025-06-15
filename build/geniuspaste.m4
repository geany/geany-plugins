AC_DEFUN([GP_CHECK_GENIUSPASTE],
[
    GP_ARG_DISABLE([GeniusPaste], [auto])

    GP_CHECK_PLUGIN_DEPS([GeniusPaste], GENIUSPASTE,
                         [gtk+-3.0 >= 3.16
                          libsoup-3.0])

    GP_COMMIT_PLUGIN_STATUS([GeniusPaste])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
