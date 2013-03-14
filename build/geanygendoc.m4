AC_DEFUN([GP_CHECK_GEANYGENDOC],
[
    GP_ARG_DISABLE([GeanyGenDoc], [auto])

    GTK_VERSION=2.12
    GLIB_VERSION=2.14
    GIO_VERSION=2.18
    CTPL_VERSION=0.3

    GP_CHECK_PLUGIN_DEPS([GeanyGenDoc], GEANYGENDOC,
                         [$GP_GTK_PACKAGE >= ${GTK_VERSION}
                          glib-2.0 >= ${GLIB_VERSION}
                          gio-2.0 >= ${GIO_VERSION}
                          ctpl >= ${CTPL_VERSION}])

    AC_PATH_PROG([RST2HTML], [rst2html], [no])
    AC_SUBST([RST2HTML])
    AM_CONDITIONAL([BUILD_RST], [test "x$RST2HTML" != "xno"])

    GP_COMMIT_PLUGIN_STATUS([GeanyGenDoc])

    AC_CONFIG_FILES([
        geanygendoc/Makefile
        geanygendoc/src/Makefile
        geanygendoc/data/Makefile
        geanygendoc/data/filetypes/Makefile
        geanygendoc/docs/Makefile
    ])
])
