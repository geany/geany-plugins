AC_DEFUN([GP_CHECK_WEBHELPER],
[
    GP_ARG_DISABLE([WebHelper], [auto])

    GTK_VERSION=2.16
    GLIB_VERSION=2.16
    GIO_VERSION=2.18
    GDK_PIXBUF_VERSION=2.0
    WEBKIT_VERSION=1.1.18

    AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums], [no])
    AC_SUBST([GLIB_MKENUMS])
    if [[ x"$GLIB_MKENUMS" = "xno" ]]; then
        if [[ x"$enable_webhelper" = "xyes" ]]; then
            AC_MSG_ERROR([glib-mkenums not found, are GLib dev tools installed?])
        else
            enable_webhelper=no
        fi
    fi

    GP_CHECK_GTK3([webkit_package=webkitgtk-3.0],
                  [webkit_package=webkit-1.0])
    GP_CHECK_PLUGIN_DEPS([WebHelper], [WEBHELPER],
                         [$GP_GTK_PACKAGE >= ${GTK_VERSION}
                          glib-2.0 >= ${GLIB_VERSION}
                          gio-2.0 >= ${GIO_VERSION}
                          gdk-pixbuf-2.0 >= ${GDK_PIXBUF_VERSION}
                          $webkit_package >= ${WEBKIT_VERSION}
                          gthread-2.0])


    GP_COMMIT_PLUGIN_STATUS([WebHelper])

    AC_CONFIG_FILES([
        webhelper/Makefile
        webhelper/src/Makefile
    ])
])
