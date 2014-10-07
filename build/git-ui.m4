AC_DEFUN([GP_CHECK_GITUI],
[
    GP_ARG_DISABLE([GitUI], [auto])

    GP_CHECK_PLUGIN_DEPS([GitUI], [GITUI],
                         [$GP_GTK_PACKAGE >= 2.18
                          glib-2.0
                          libgit2])

    GP_COMMIT_PLUGIN_STATUS([GitUI])

    AC_CONFIG_FILES([
        git-ui/Makefile
        git-ui/src/Makefile
    ])
])
