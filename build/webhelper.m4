AC_DEFUN([GP_CHECK_WEBHELPER],
[
    GP_ARG_DISABLE([WebHelper], [auto])

    GTK_VERSION=3.0
    GLIB_VERSION=2.38
    GIO_VERSION=2.30
    GDK_PIXBUF_VERSION=2.0
    WEBKIT_VERSION=2.18

    AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums], [no])
    AC_SUBST([GLIB_MKENUMS])
    if [[ x"$GLIB_MKENUMS" = "xno" ]]; then
        if [[ x"$enable_webhelper" = "xyes" ]]; then
            AC_MSG_ERROR([glib-mkenums not found, are GLib dev tools installed?])
        else
            enable_webhelper=no
        fi
    fi

    dnl Support both webkit2gtk 4.0 and 4.1, as the only difference is the
    dnl libsoup version in the API, which we don't use.
    dnl Prefer the 4.1 version, but use the 4.0 version as fallback if
    dnl available -- yet still ask for the 4.1 if neither are available
    webkit_package=webkit2gtk-4.1
    PKG_CHECK_EXISTS([${webkit_package} >= ${WEBKIT_VERSION}],,
                     [PKG_CHECK_EXISTS([webkit2gtk-4.0 >= ${WEBKIT_VERSION}],
                                       [webkit_package=webkit2gtk-4.0])])

    GP_CHECK_PLUGIN_GTK3_ONLY([webhelper])
    GP_CHECK_PLUGIN_DEPS([WebHelper], [WEBHELPER],
                         [$GP_GTK_PACKAGE >= ${GTK_VERSION}
                          glib-2.0 >= ${GLIB_VERSION}
                          gio-2.0 >= ${GIO_VERSION}
                          gdk-pixbuf-2.0 >= ${GDK_PIXBUF_VERSION}
                          $webkit_package >= ${WEBKIT_VERSION}
                          gthread-2.0])


    GP_COMMIT_PLUGIN_STATUS([WebHelper])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
