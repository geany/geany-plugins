AC_DEFUN([GP_CHECK_COMMANDER],
[
    GP_ARG_DISABLE([Commander], [auto])

    GP_CHECK_PLUGIN_DEPS([Commander], [COMMANDER],
                         [gtk+-2.0 >= 2.0
                          glib-2.0 >= 2.0])

    GP_STATUS_PLUGIN_ADD([Commander], [$enable_commander])

    AC_CONFIG_FILES([
        commander/Makefile
        commander/src/Makefile
    ])
])
