AC_DEFUN([GP_CHECK_GITCHANGEBAR],
[
    GP_ARG_DISABLE([GitChangeBar], [auto])

    GP_CHECK_PLUGIN_DEPS([GitChangeBar], [GITCHANGEBAR],
                         [$GP_GTK_PACKAGE >= 2.18
                          glib-2.0
                          libgit2 >= 0.21])

    GP_COMMIT_PLUGIN_STATUS([GitChangeBar])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
