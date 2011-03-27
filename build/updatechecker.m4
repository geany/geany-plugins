AC_DEFUN([GP_CHECK_UPDATECHECKER],
[
    GP_ARG_DISABLE([Updatechecker], [auto])

    GP_CHECK_PLUGIN_DEPS([Updatechecker], UPDATECHECKER,
                         [libsoup-2.4 >= 2.4.0
                          gthread-2.0])

    GP_STATUS_PLUGIN_ADD([Updatechecker], [$enable_updatechecker])

    AC_CONFIG_FILES([
        updatechecker/Makefile
        updatechecker/src/Makefile
    ])
])
