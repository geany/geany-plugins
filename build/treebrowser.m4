AC_DEFUN([GP_CHECK_TREEBROWSER],
[
    AC_ARG_ENABLE(treebrowser,
        AC_HELP_STRING([--enable-treebrowser=ARG],
            [Enable TreeBrowser plugin [[default=auto]]]),,
        [enable_treebrowser=auto])

    treebrowser_have_creat=yes
    AC_CHECK_HEADERS([sys/types.h sys/stat.h fcntl.h],
                     [], [treebrowser_have_creat=no])
    AC_CHECK_FUNC([creat], [], [treebrowser_have_creat=no])
    PKG_CHECK_MODULES([GIO], [gio-2.0],
        [AC_DEFINE([HAVE_GIO], 1, [Whether we have GIO])],
        [AC_MSG_NOTICE([Treebrowser GIO support is disabled because of the following problem: $GIO1_PKG_ERRORS])])

    if test "x$enable_treebrowser" = "xauto"; then
        enable_treebrowser="$treebrowser_have_creat"
    elif test "x$enable_treebrowser" = "xyes"; then
        if ! test "x$treebrowser_have_creat" = "xyes"; then
            AC_MSG_ERROR([missing function creat()])
        fi
    fi

    AM_CONDITIONAL(ENABLE_TREEBROWSER, test "x$enable_treebrowser" = "xyes")
    GP_STATUS_PLUGIN_ADD([TreeBrowser], [$enable_treebrowser])

    AC_CONFIG_FILES([
        treebrowser/Makefile
        treebrowser/src/Makefile
    ])
])
