AC_DEFUN([GP_CHECK_GEANYGENDOC],
[
    AC_ARG_ENABLE(geanygendoc,
        AC_HELP_STRING([--enable-geanygendoc=ARG],
            [Enable GeanyGenDoc plugin [[default=auto]]]),,
        [enable_geanygendoc=auto])

    GTK_VERSION=2.12
    GLIB_VERSION=2.14
    GIO_VERSION=2.18
    CTPL_VERSION=0.2

    geanygendoc_have_gtk=no
    geanygendoc_have_glib=no
    geanygendoc_have_gio=no
    geanygendoc_have_ctpl=no
    if [[ x"$enable_geanygendoc" = "xauto" ]]; then
        PKG_CHECK_MODULES(GTK,  [gtk+-2.0 >= ${GTK_VERSION}],
                          [geanygendoc_have_gtk=yes],
                          [geanygendoc_have_gtk=no])
        PKG_CHECK_MODULES(GLIB, [glib-2.0 >= ${GLIB_VERSION}],
                          [geanygendoc_have_glib=yes],
                          [geanygendoc_have_glib=no])
        PKG_CHECK_MODULES(GIO,  [gio-2.0 >= ${GIO_VERSION}],
                          [geanygendoc_have_gio=yes],
                          [geanygendoc_have_gio=no])
        PKG_CHECK_MODULES(CTPL, [ctpl >= ${CTPL_VERSION}],
                          [geanygendoc_have_ctpl=yes],
                          [geanygendoc_have_ctpl=no])
        if [[ $geanygendoc_have_gtk  = yes ]] &&
           [[ $geanygendoc_have_glib = yes ]] &&
           [[ $geanygendoc_have_gio  = yes ]] &&
           [[ $geanygendoc_have_ctpl = yes ]]; then
            enable_geanygendoc=yes
        else
            enable_geanygendoc=no
        fi
    elif [[ x"$enable_geanygendoc" = "xyes" ]]; then
        PKG_CHECK_MODULES(GTK,  [gtk+-2.0 >= ${GTK_VERSION}])
        PKG_CHECK_MODULES(GLIB, [glib-2.0 >= ${GLIB_VERSION}])
        PKG_CHECK_MODULES(GIO,  [gio-2.0 >= ${GIO_VERSION}])
        PKG_CHECK_MODULES(CTPL, [ctpl >= ${CTPL_VERSION}])
    fi

    AC_PATH_PROG([RST2HTML], [rst2html], [no])
    AC_SUBST([RST2HTML])
    AM_CONDITIONAL([BUILD_RST], [test "x$RST2HTML" != "xno"])

    AM_CONDITIONAL(ENABLE_GEANYGENDOC, test $enable_geanygendoc = yes)
    GP_STATUS_PLUGIN_ADD([GeanyGenDoc], [$enable_geanygendoc])

    AC_CONFIG_FILES([
        geanygendoc/Makefile
        geanygendoc/src/Makefile
        geanygendoc/data/Makefile
        geanygendoc/data/filetypes/Makefile
        geanygendoc/docs/Makefile
    ])
])
