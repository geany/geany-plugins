AC_DEFUN([GP_CHECK_GEANYMACRO],
[
    GP_ARG_DISABLE([GeanyMacro], [yes])
    GP_COMMIT_PLUGIN_STATUS([GeanyMacro])
    AC_CONFIG_FILES([
        geanymacro/Makefile
        geanymacro/src/Makefile
    ])
])
