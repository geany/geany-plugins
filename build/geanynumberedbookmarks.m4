AC_DEFUN([GP_CHECK_GEANYNUMBEREDBOOKMARKS],
[
    GP_ARG_DISABLE([GeanyNumberedBookmarks], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyNumberedBookmarks])
    AC_CONFIG_FILES([
        geanynumberedbookmarks/Makefile
        geanynumberedbookmarks/src/Makefile
    ])
])
