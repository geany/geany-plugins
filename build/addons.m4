AC_DEFUN([GP_CHECK_ADDONS],
[
    GP_ARG_DISABLE(Addons, yes)
    GP_COMMIT_PLUGIN_STATUS([Addons])
    AC_CONFIG_FILES([
        addons/Makefile
        addons/src/Makefile
    ])
])
