AC_DEFUN([GP_CHECK_DEVHELP],
[
    GP_ARG_DISABLE([devhelp], [auto])

    GTK_VERSION=2.16
    GLIB_VERSION=2.16
    WEBKIT_VERSION=1.1.18
    DEVHELP1_VERSION=2.30.1
    DEVHELP2_VERSION=2.32.0

    GP_CHECK_PLUGIN_DEPS([devhelp], [DHPLUG],
						 [gtk+-2.0 >= ${GTK_VERSION}
						  glib-2.0 >= ${GLIB_VERSION}
						  webkit-1.0 >= ${WEBKIT_VERSION}
						  gthread-2.0])

    # First tries newer library version libdevhelp-2.0 and if that
    # fails, tries to find older version libdevhelp-1.0.
    if test "$m4_tolower(AS_TR_SH(enable_devhelp))" = "yes"; then
        PKG_CHECK_MODULES(
            [DEVHELP], 
            [libdevhelp-2.0 >= ${DEVHELP2_VERSION}], 
            [AC_DEFINE([HAVE_BOOK_MANAGER], [1], [Use libdevhelp-2.0])],
            [PKG_CHECK_MODULES(
                [DEVHELP], 
                [libdevhelp-1.0 >= ${DEVHELP1_VERSION}])
            ]
        )
    elif test "$m4_tolower(AS_TR_SH(enable_devhelp))" = "auto"; then
        PKG_CHECK_MODULES(
            [DEVHELP],
            [libdevhelp-2.0 >= ${DEVHELP2_VERSION}],
            [AC_DEFINE(HAVE_BOOK_MANAGER)],
            [PKG_CHECK_MODULES(
                    [DEVHELP],
                    [libdevhelp-1.0 >= ${DEVHELP1_VERSION}],
                    [m4_tolower(AS_TR_SH(enable_$1))=yes],
                    [m4_tolower(AS_TR_SH(enable_$1))=no])
            ]
        )
    fi

    AM_CONDITIONAL(m4_toupper(AS_TR_SH(ENABLE_$1)),
                   test "$m4_tolower(AS_TR_SH(enable_$1))" = yes)

    GP_STATUS_PLUGIN_ADD([DevHelp], [$enable_devhelp])

    AC_CONFIG_FILES([
        devhelp/Makefile
        devhelp/src/Makefile
        devhelp/data/Makefile
    ])
])
