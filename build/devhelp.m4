AC_DEFUN([GP_CHECK_DEVHELP],
[
    GP_ARG_DISABLE([devhelp], [auto])

    GTK_VERSION=2.16
    WEBKIT_VERSION=1.1.13
    GCONF_VERSION=2.6.0
    LIBWNCK_VERSION=2.10.0

    AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
    AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

    GP_CHECK_PLUGIN_DEPS([devhelp], [DEVHELP],
                         [gtk+-2.0 >= ${GTK_VERSION}
                          webkit-1.0 >= ${WEBKIT_VERSION}
                          libwnck-1.0 >= ${LIBWNCK_VERSION}
                          gconf-2.0 >= ${GCONF_VERSION}
                          gthread-2.0
                          zlib])

    GP_COMMIT_PLUGIN_STATUS([DevHelp])

    AC_CONFIG_FILES([
        devhelp/Makefile
        devhelp/devhelp/Makefile
        devhelp/src/Makefile
        devhelp/data/Makefile
    ])
])
