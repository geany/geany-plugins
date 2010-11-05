AC_DEFUN([GP_CHECK_ADDONS],
[
    GP_ARG_DISABLE(Addons, yes)
    GP_STATUS_PLUGIN_ADD([Addons], [$enable_addons])
    AC_CONFIG_FILES([
        addons/Makefile
        addons/src/Makefile
    ])
])
