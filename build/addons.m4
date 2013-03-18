AC_DEFUN([GP_CHECK_ADDONS],
[
    GP_ARG_DISABLE([Addons], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([Addons])
    GP_COMMIT_PLUGIN_STATUS([Addons])
    AC_CONFIG_FILES([
        addons/Makefile
        addons/src/Makefile
    ])
])
