AC_DEFUN([GP_CHECK_UPDATECHECKER],
[
    GP_ARG_DISABLE([Updatechecker], [auto])

    GP_CHECK_PLUGIN_DEPS([Updatechecker], UPDATECHECKER,
                         [libsoup-3.0])

    GP_COMMIT_PLUGIN_STATUS([Updatechecker])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
