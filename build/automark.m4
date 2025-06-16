AC_DEFUN([GP_CHECK_AUTOMARK],
[
    GP_ARG_DISABLE([Automark], [auto])
    GP_CHECK_PLUGIN_DEPS([Automark], [AUTOMARK],
                         [$GP_GTK_PACKAGE >= 2.8])
    GP_COMMIT_PLUGIN_STATUS([Automark])
    AC_CONFIG_FILES([
        automark/Makefile
        automark/src/Makefile
    ])
])
