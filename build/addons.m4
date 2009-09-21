AC_DEFUN([GP_CHECK_ADDONS],
[
    GP_STATUS_PLUGIN_ADD([Addons], [yes])
    AC_CONFIG_FILES([
        addons/Makefile
        addons/src/Makefile
    ])
])
