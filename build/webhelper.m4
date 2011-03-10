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

    GP_CHECK_PLUGIN_DEPS([WebHelper], [WEBHELPER],
                         [gtk+-2.0 >= ${GTK_VERSION}
                          glib-2.0 >= ${GLIB_VERSION}
                          gio-2.0 >= ${GIO_VERSION}
                          gdk-pixbuf-2.0 >= ${GDK_PIXBUF_VERSION}
                          webkit-1.0 >= ${WEBKIT_VERSION}
                          gthread-2.0])


    GP_STATUS_PLUGIN_ADD([WebHelper], [$enable_webhelper])

    AC_CONFIG_FILES([
        webhelper/Makefile
        webhelper/src/Makefile
    ])
])
