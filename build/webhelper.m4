AC_DEFUN([GP_CHECK_WEBHELPER],
[
    AC_ARG_ENABLE([webhelper],
                  AC_HELP_STRING([--enable-webhelper=ARG],
                                 [Enable WebHelper plugin [[default=auto]]]),
                  [],
                  [enable_webhelper=auto])

    GTK_VERSION=2.16
    GLIB_VERSION=2.16
    GIO_VERSION=2.18
    GDK_PIXBUF_VERSION=2.0
    WEBKIT_VERSION=1.1.18

    webhelper_have_gtk=no
    webhelper_have_glib=no
    webhelper_have_gio=no
    webhelper_have_gdk_pixbuf=no
    webhelper_have_webkit=no
    if [[ x"$enable_webhelper" = "xauto" ]]; then
        PKG_CHECK_MODULES([GTK],  [gtk+-2.0 >= ${GTK_VERSION}],
                          [webhelper_have_gtk=yes],
                          [webhelper_have_gtk=no])
        PKG_CHECK_MODULES([GLIB], [glib-2.0 >= ${GLIB_VERSION}],
                          [webhelper_have_glib=yes],
                          [webhelper_have_glib=no])
        PKG_CHECK_MODULES([GIO],  [gio-2.0 >= ${GIO_VERSION}],
                          [webhelper_have_gio=yes],
                          [webhelper_have_gio=no])
        PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0 >= ${GDK_PIXBUF_VERSION}],
                          [webhelper_have_gdk_pixbuf=yes],
                          [webhelper_have_gdk_pixbuf=no])
        PKG_CHECK_MODULES([WEBKIT], [webkit-1.0 >= ${WEBKIT_VERSION}],
                          [webhelper_have_webkit=yes],
                          [webhelper_have_webkit=no])
        if [[ $webhelper_have_gtk         = yes ]] &&
           [[ $webhelper_have_glib        = yes ]] &&
           [[ $webhelper_have_gio         = yes ]] &&
           [[ $webhelper_have_gdk_pixbuf  = yes ]] &&
           [[ $webhelper_have_webkit      = yes ]]; then
            enable_webhelper=yes
        else
            enable_webhelper=no
        fi
    elif [[ x"$enable_webhelper" = "xyes" ]]; then
        PKG_CHECK_MODULES([GTK],        [gtk+-2.0 >= ${GTK_VERSION}])
        PKG_CHECK_MODULES([GLIB],       [glib-2.0 >= ${GLIB_VERSION}])
        PKG_CHECK_MODULES([GIO],        [gio-2.0 >= ${GIO_VERSION}])
        PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0 >= ${GDK_PIXBUF_VERSION}])
        PKG_CHECK_MODULES([WEBKIT],     [webkit-1.0 >= ${WEBKIT_VERSION}])
    fi

    AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums], [no])
    AC_SUBST([GLIB_MKENUMS])
    if [[ x"$GLIB_MKENUMS" = "xno" ]]; then
        if [[ x"$enable_webhelper" = "xyes" ]]; then
            AC_MSG_ERROR([glib-mkenums not found, are GLib dev tools installed?])
        else
            enable_webhelper=no
        fi
    fi

    AM_CONDITIONAL(ENABLE_WEBHELPER, test $enable_webhelper = yes)
    GP_STATUS_PLUGIN_ADD([WebHelper], [$enable_webhelper])

    AC_CONFIG_FILES([
        webhelper/Makefile
        webhelper/src/Makefile
    ])
])
