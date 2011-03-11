AC_DEFUN([GP_CHECK_DEVHELP],
[
    GP_ARG_DISABLE([devhelp], [auto])
    GP_CHECK_PLUGIN_DEPS([devhelp], [DEVHELP], 
		[libdevhelp-1.0 webkit-1.0 gthread-2.0])
    GP_STATUS_PLUGIN_ADD([DevHelp], [$enable_devhelp])
    AC_CONFIG_FILES([
        devhelp/Makefile
        devhelp/src/Makefile
        devhelp/data/Makefile
    ])
])
