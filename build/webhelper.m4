AC_DEFUN([GP_CHECK_WEBHELPER],
[
    GP_ARG_DISABLE([WebHelper], [auto])

    GTK_VERSION=2.16
    dnl 2.22 for glib-mkenums' @basename@ template
    GLIB_VERSION=2.22
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

    GP_CHECK_PLUGIN_GTK3_ONLY([webhelper])
    GP_CHECK_PLUGIN_DEPS([WebHelper], [WEBHELPER],
                         [$GP_GTK_PACKAGE >= ${GTK_VERSION}
                          glib-2.0 >= ${GLIB_VERSION}
                          gio-2.0 >= ${GIO_VERSION}
                          gdk-pixbuf-2.0 >= ${GDK_PIXBUF_VERSION}
                          webkit2gtk-4.0 >= ${WEBKIT_VERSION}
                          gthread-2.0])


    GP_COMMIT_PLUGIN_STATUS([WebHelper])

    AC_CONFIG_FILES([
        webhelper/Makefile
        webhelper/src/Makefile
    ])
])
