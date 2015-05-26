AC_DEFUN([GP_CHECK_GITCHANGEBAR],
[
    GP_ARG_DISABLE([GitChangeBar], [auto])

    GP_CHECK_PLUGIN_DEPS([GitChangeBar], [GITCHANGEBAR],
                         [$GP_GTK_PACKAGE >= 2.18
                          glib-2.0
                          libgit2 >= 0.21])

    GP_COMMIT_PLUGIN_STATUS([GitChangeBar])

    AC_CONFIG_FILES([
        git-changebar/Makefile
        git-changebar/data/Makefile
        git-changebar/src/Makefile
    ])
])
