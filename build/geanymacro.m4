AC_DEFUN([GP_CHECK_GEANYMACRO],
[
    GP_ARG_DISABLE([GeanyMacro], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyMacro], [$enable_geanymacro])
    AC_CONFIG_FILES([
        geanymacro/Makefile
        geanymacro/src/Makefile
    ])
])
