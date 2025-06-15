AC_DEFUN([GP_CHECK_GEANYNUMBEREDBOOKMARKS],
[
    GP_ARG_DISABLE([GeanyNumberedBookmarks], [auto])
    GP_CHECK_UTILSLIB([GeanyNumberedBookmarks])

    GP_COMMIT_PLUGIN_STATUS([GeanyNumberedBookmarks])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
