AC_DEFUN([GP_CHECK_OVERVIEW],
[
    GP_ARG_DISABLE([overview], [auto])
    GP_COMMIT_PLUGIN_STATUS([Overview])
    AC_CONFIG_FILES([build/overview/Makefile])
])
