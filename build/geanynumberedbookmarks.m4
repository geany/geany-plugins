AC_DEFUN([GP_CHECK_GEANYNUMBEREDBOOKMARKS],
[
    GP_ARG_DISABLE([GeanyNumberedBookmarks], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyNumberedBookmarks], [$enable_geanynumberedbookmarks])
    AC_CONFIG_FILES([
        geanynumberedbookmarks/Makefile
        geanynumberedbookmarks/src/Makefile
    ])
])
