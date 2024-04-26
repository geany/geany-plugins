AC_DEFUN([GP_CHECK_UPDATECHECKER],
[
    GP_ARG_DISABLE([Updatechecker], [auto])

    GP_CHECK_PLUGIN_DEPS([Updatechecker], UPDATECHECKER,
                         [gio-2.0 >= 2.44])

    GP_COMMIT_PLUGIN_STATUS([Updatechecker])

    AC_CONFIG_FILES([
        updatechecker/Makefile
        updatechecker/src/Makefile
    ])
])
