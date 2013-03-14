AC_DEFUN([GP_CHECK_GEANYNUMBEREDBOOKMARKS],
[
    GP_ARG_DISABLE([GeanyNumberedBookmarks], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyNumberedBookmarks])
    GP_COMMIT_PLUGIN_STATUS([GeanyNumberedBookmarks])
    AC_CONFIG_FILES([
        geanynumberedbookmarks/Makefile
        geanynumberedbookmarks/src/Makefile
    ])
])
