AC_DEFUN([GP_CHECK_GEANYMACRO],
[
    GP_ARG_DISABLE([GeanyMacro], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyMacro])
    GP_COMMIT_PLUGIN_STATUS([GeanyMacro])
    AC_CONFIG_FILES([
        geanymacro/Makefile
        geanymacro/src/Makefile
    ])
])
