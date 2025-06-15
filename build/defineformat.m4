AC_DEFUN([GP_CHECK_DEFINEFORMAT],
[
    GP_ARG_DISABLE([Defineformat], [auto])
    GP_CHECK_PLUGIN_DEPS([Defineformat], [DEFINEFORMAT],
                         [$GP_GTK_PACKAGE >= 2.8])
    GP_COMMIT_PLUGIN_STATUS([Defineformat])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
