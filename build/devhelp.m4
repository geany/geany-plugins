AC_DEFUN([GP_CHECK_DEVHELP],
[
    GP_ARG_DISABLE([devhelp], [auto])

    GTK_VERSION=2.16
    GLIB_VERSION=2.16
    WEBKIT_VERSION=1.1.18
    DEVHELP1_VERSION=2.30.1
    DEVHELP2_VERSION=2.32.0

    # Use newer libdevhelp-2.0 if present, and fallback on older libdevhelp-1.0
    libdevhelp_pkg=libdevhelp-2.0
    libdevhelp_version=${DEVHELP2_VERSION}
    AS_IF([test "x$enable_devhelp" != "xno"],
          [PKG_CHECK_EXISTS([${libdevhelp_pkg} >= ${libdevhelp_version}],
                            [],
                            [libdevhelp_pkg=libdevhelp-1.0
                             libdevhelp_version=${DEVHELP1_VERSION}])])

    GP_CHECK_PLUGIN_DEPS([devhelp], [DEVHELP],
                         [gtk+-2.0 >= ${GTK_VERSION}
                          glib-2.0 >= ${GLIB_VERSION}
                          webkit-1.0 >= ${WEBKIT_VERSION}
                          ${libdevhelp_pkg} >= ${libdevhelp_version}
                          gthread-2.0])

    GP_STATUS_PLUGIN_ADD([DevHelp], [$enable_devhelp])

    AC_CONFIG_FILES([
        devhelp/Makefile
        devhelp/src/Makefile
        devhelp/data/Makefile
    ])
])
