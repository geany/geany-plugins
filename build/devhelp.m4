AC_DEFUN([GP_CHECK_DEVHELP],
[
    GP_ARG_DISABLE([devhelp], [auto])

    GTK_VERSION=2.16
    GLIB_VERSION=2.16
    WEBKIT_VERSION=1.1.18
    DEVHELP_VERSION=2.30.1

    GP_CHECK_PLUGIN_DEPS([devhelp], [DEVHELP],
						 [gtk+-2.0 >= ${GTK_VERSION}
						  glib-2.0 >= ${GLIB_VERSION}
						  webkit-1.0 >= ${WEBKIT_VERSION}
						  libdevhelp-1.0 >= ${DEVHELP_VERSION}
						  gthread-2.0])

    GP_STATUS_PLUGIN_ADD([DevHelp], [$enable_devhelp])

    AC_CONFIG_FILES([
        devhelp/Makefile
        devhelp/src/Makefile
        devhelp/data/Makefile
    ])
])
