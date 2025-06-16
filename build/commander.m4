AC_DEFUN([GP_CHECK_COMMANDER],
[
    GP_ARG_DISABLE([Commander], [auto])

    GP_CHECK_PLUGIN_DEPS([Commander], [COMMANDER],
                         [$GP_GTK_PACKAGE >= 2.16
                          glib-2.0 >= 2.4])

    GP_COMMIT_PLUGIN_STATUS([Commander])

    AC_CONFIG_FILES([
        commander/Makefile
        commander/src/Makefile
    ])
])
