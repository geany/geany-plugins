AC_DEFUN([GP_CHECK_DEVHELP],
[
    GP_ARG_DISABLE([devhelp], [auto])
    AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
    AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)
    GP_CHECK_GTK3(
      [
        AC_DEFINE([HAVE_DEVHELP_GTK3], [1], [Define if using GTK3 Devhelp plugin])
      ], [
        WEBKIT_VERSION=1.1.13
        DEVHELP_VERSION=3.0
        GCONF_VERSION=2.6.0
        LIBWNCK_VERSION=2.10.0
        GP_CHECK_PLUGIN_DEPS([devhelp], [DEVHELP],
                             [libwebkit2gtk-4.0 >= ${WEBKIT_VERSION}
                             libdevhelp-3.0 >= {DEVHELP_VERSION}
                             gconf-2.0 >= ${GCONF_VERSION}
                             libwnck-1.0 >= ${LIBWNCK_VERSION}
                             gthread-2.0
                             zlib])
      ]
    )
    GP_COMMIT_PLUGIN_STATUS([DevHelp])
    AC_CONFIG_FILES([
        devhelp/Makefile
        devhelp/src/Makefile
        devhelp/data/Makefile
    ])
])
